#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

// ponytail: basic INI parser covers target + env sections with UTF-8 BOM strip.
// upgrade path: replace parse_ini with full INI library if nested sections or multiline values needed.

struct Config {
    std::wstring executable;
    std::wstring arguments;
    std::wstring working_dir;
    bool verbose = false;
    std::vector<std::pair<std::wstring, std::wstring>> env_vars;
};

static std::wstring trim(const std::wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::wstring to_lower(std::wstring s) {
    for (auto& c : s) c = towlower(c);
    return s;
}

static std::wstring strip_quotes(const std::wstring& s) {
    if (s.size() >= 2) {
        if ((s.front() == L'"' && s.back() == L'"') ||
            (s.front() == L'\'' && s.back() == L'\'')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

static std::wstring expand_env(const std::wstring& str) {
    DWORD needed = ExpandEnvironmentStringsW(str.c_str(), NULL, 0);
    if (needed == 0) return str;
    std::wstring out(needed, L'\0');
    ExpandEnvironmentStringsW(str.c_str(), out.data(), needed);
    while (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

static std::wstring utf8_to_wide(const std::string& str) {
    if (str.empty()) return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
    if (needed <= 0) return L"";
    std::wstring out(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), out.data(), needed);
    return out;
}

static bool parse_ini(const fs::path& ini_path, Config& cfg) {
    std::ifstream file(ini_path, std::ios::binary);
    if (!file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    std::string_view view(content);
    if (view.size() >= 3 &&
        static_cast<unsigned char>(view[0]) == 0xEF &&
        static_cast<unsigned char>(view[1]) == 0xBB &&
        static_cast<unsigned char>(view[2]) == 0xBF) {
        view.remove_prefix(3);
    }

    std::stringstream ss{ std::string(view) };
    std::string line;
    std::wstring current_section;

    while (std::getline(ss, line)) {
        std::wstring wline = trim(utf8_to_wide(line));
        if (wline.empty() || wline[0] == L';' || wline[0] == L'#') continue;

        if (wline.front() == L'[' && wline.back() == L']') {
            current_section = to_lower(trim(wline.substr(1, wline.size() - 2)));
            continue;
        }

        size_t eq = wline.find(L'=');
        if (eq == std::wstring::npos) continue;

        std::wstring key = trim(wline.substr(0, eq));
        std::wstring val = strip_quotes(trim(wline.substr(eq + 1)));
        std::wstring key_lower = to_lower(key);

        if (current_section == L"target" || current_section == L"app") {
            if (key_lower == L"executable" || key_lower == L"path" || key_lower == L"cmd") {
                cfg.executable = val;
            } else if (key_lower == L"arguments" || key_lower == L"args") {
                cfg.arguments = val;
            } else if (key_lower == L"working_dir" || key_lower == L"workdir" || key_lower == L"cwd") {
                cfg.working_dir = val;
            } else if (key_lower == L"verbose") {
                cfg.verbose = (to_lower(val) == L"true" || val == L"1");
            }
        } else if (current_section == L"env" || current_section == L"environment") {
            cfg.env_vars.push_back({ key, val });
        }
    }
    return true;
}

static fs::path get_exe_dir() {
    wchar_t exe_buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exe_buf, MAX_PATH);
    if (len > 0) {
        return fs::path(exe_buf).parent_path();
    }
    return fs::current_path();
}

static fs::path resolve_ini_path(const std::wstring& custom_path, const fs::path& exe_dir) {
    if (!custom_path.empty()) {
        fs::path p(custom_path);
        if (fs::exists(p)) return fs::absolute(p);
        return p;
    }

    fs::path beside_exe = exe_dir / L"config.ini";
    if (fs::exists(beside_exe)) return beside_exe;

    fs::path cwd_ini = fs::current_path() / L"config.ini";
    if (fs::exists(cwd_ini)) return cwd_ini;

    return L"config.ini";
}

int wmain(int argc, wchar_t* argv[]) {
    std::wstring custom_ini;
    std::wstring extra_args;
    bool force_verbose = false;

    // Parse wrapper CLI arguments
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--config" || arg == L"-c") {
            if (i + 1 < argc) {
                custom_ini = argv[++i];
            } else {
                std::wcerr << L"Error: --config requires a file path argument.\n";
                return 1;
            }
        } else if (arg == L"--verbose" || arg == L"-v") {
            force_verbose = true;
        } else if (arg == L"--help" || arg == L"-h") {
            std::wcout << L"Usage: wrapper.exe [options] [target_args...]\n"
                       << L"Options:\n"
                       << L"  -c, --config <file>  Specify path to config.ini\n"
                       << L"  -v, --verbose        Print wrapper diagnostics\n"
                       << L"  -h, --help           Show this help message\n";
            return 0;
        } else {
            // Forward remaining arguments
            if (!extra_args.empty()) extra_args += L" ";
            if (arg.find(L' ') != std::wstring::npos && arg.front() != L'"') {
                extra_args += L"\"" + arg + L"\"";
            } else {
                extra_args += arg;
            }
        }
    }

    fs::path exe_dir = get_exe_dir();
    SetEnvironmentVariableW(L"DIR", exe_dir.c_str());

    fs::path ini_path = resolve_ini_path(custom_ini, exe_dir);
    Config cfg;
    if (!parse_ini(ini_path, cfg)) {
        std::wcerr << L"Error: Failed to open configuration file: " << ini_path.wstring() << L"\n";
        return 1;
    }

    if (force_verbose) {
        cfg.verbose = true;
    }

    if (cfg.verbose) {
        std::wcout << L"[wrapper:dir] DIR=" << exe_dir.wstring() << L"\n";
    }

    // Apply wrapped environment variables (machine env + custom config.ini env)
    for (const auto& [key, raw_val] : cfg.env_vars) {
        std::wstring expanded = expand_env(raw_val);
        SetEnvironmentVariableW(key.c_str(), expanded.c_str());
        if (cfg.verbose) {
            std::wcout << L"[wrapper:env] " << key << L"=" << expanded << L"\n";
        }
    }

    if (cfg.executable.empty()) {
        if (extra_args.empty()) {
            std::wcerr << L"Error: No executable specified in config.ini [target] or CLI.\n";
            return 1;
        }
        // First CLI token is executable, remainder are args
        size_t sp = extra_args.find(L' ');
        if (sp != std::wstring::npos) {
            cfg.executable = extra_args.substr(0, sp);
            cfg.arguments = extra_args.substr(sp + 1);
        } else {
            cfg.executable = extra_args;
            cfg.arguments.clear();
        }
        extra_args.clear();
    }

    // Expand environment variables (including %DIR%) in target configuration
    cfg.executable = expand_env(cfg.executable);
    cfg.arguments = expand_env(cfg.arguments);

    // Combine config args and extra CLI args
    std::wstring final_args = cfg.arguments;
    if (!extra_args.empty()) {
        if (!final_args.empty()) final_args += L" ";
        final_args += extra_args;
    }

    // Build command line for CreateProcessW
    std::wstring cmd_line;
    if (cfg.executable.front() != L'"' && cfg.executable.find(L' ') != std::wstring::npos) {
        cmd_line = L"\"" + cfg.executable + L"\"";
    } else {
        cmd_line = cfg.executable;
    }

    if (!final_args.empty()) {
        cmd_line += L" " + final_args;
    }

    const wchar_t* work_dir_ptr = nullptr;
    std::wstring resolved_work_dir;
    if (!cfg.working_dir.empty()) {
        resolved_work_dir = expand_env(cfg.working_dir);
        work_dir_ptr = resolved_work_dir.c_str();
    }

    if (cfg.verbose) {
        std::wcout << L"[wrapper:run] Executable: " << cfg.executable << L"\n"
                   << L"[wrapper:run] Command line: " << cmd_line << L"\n"
                   << L"[wrapper:run] Working dir: " << (work_dir_ptr ? work_dir_ptr : L"(inherited)") << L"\n";
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    // Ignore Ctrl+C in wrapper so child receives and handles signal first
    SetConsoleCtrlHandler(NULL, TRUE);

    BOOL success = CreateProcessW(
        NULL,
        cmd_line.data(),
        NULL,
        NULL,
        TRUE,   // Inherit handles (stdin/stdout/stderr)
        0,
        NULL,   // Inherit current process environment (machine + custom env)
        work_dir_ptr,
        &si,
        &pi
    );

    if (!success) {
        DWORD err = GetLastError();
        wchar_t* msg = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&msg), 0, NULL
        );
        std::wcerr << L"Error: Failed to launch '" << cfg.executable << L"': "
                   << (msg ? msg : L"Unknown error") << L" (Code " << err << L")\n";
        LocalFree(msg);
        return static_cast<int>(err);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return static_cast<int>(exit_code);
}
