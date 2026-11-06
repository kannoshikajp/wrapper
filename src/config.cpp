#include "config.h"

#include <windows.h>
#include <fstream>
#include <sstream>
#include <string_view>

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
    if (s.size() >= 2 &&
        ((s.front() == L'"' && s.back() == L'"') ||
         (s.front() == L'\'' && s.back() == L'\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

static std::wstring utf8_to_wide(const std::string& str) {
    if (str.empty()) return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
    if (needed <= 0) return L"";
    std::wstring out(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), out.data(), needed);
    return out;
}

std::wstring expand_env(const std::wstring& str) {
    DWORD needed = ExpandEnvironmentStringsW(str.c_str(), NULL, 0);
    if (needed == 0) return str;
    std::wstring out(needed, L'\0');
    ExpandEnvironmentStringsW(str.c_str(), out.data(), needed);
    while (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

bool parse_ini(const fs::path& ini_path, Config& cfg) {
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

fs::path get_exe_dir() {
    wchar_t exe_buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exe_buf, MAX_PATH);
    if (len > 0) return fs::path(exe_buf).parent_path();
    return fs::current_path();
}

fs::path resolve_ini_path(const std::wstring& custom_path, const fs::path& exe_dir) {
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
