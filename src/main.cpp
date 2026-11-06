#include "arguments.h"
#include "config.h"
#include "process.h"

#include <windows.h>
#include <iostream>

int wmain(int argc, wchar_t* argv[]) {
    WrapperArguments wrapper_args;
    if (!parse_wrapper_arguments(argc, argv, wrapper_args)) {
        return wrapper_args.help_requested ? 0 : 1;
    }

    fs::path exe_dir = get_exe_dir();
    SetEnvironmentVariableW(L"DIR", exe_dir.c_str());

    fs::path ini_path = resolve_ini_path(wrapper_args.custom_ini, exe_dir);
    Config cfg;
    if (!parse_ini(ini_path, cfg)) {
        std::wcerr << L"Error: Failed to open configuration file: "
                   << ini_path.wstring() << L"\n";
        return 1;
    }
    if (wrapper_args.force_verbose) cfg.verbose = true;
    if (cfg.verbose) std::wcout << L"[wrapper:dir] DIR=" << exe_dir.wstring() << L"\n";

    for (const auto& [key, raw_val] : cfg.env_vars) {
        std::wstring expanded = expand_env(raw_val);
        SetEnvironmentVariableW(key.c_str(), expanded.c_str());
        if (cfg.verbose) {
            std::wcout << L"[wrapper:env] " << key << L"=" << expanded << L"\n";
        }
    }

    if (cfg.executable.empty()) {
        if (wrapper_args.target_args.empty()) {
            std::wcerr << L"Error: No executable specified in config.ini [target] or CLI.\n";
            return 1;
        }
        cfg.executable = wrapper_args.target_args.front();
        wrapper_args.target_args.erase(wrapper_args.target_args.begin());
    }

    cfg.executable = expand_env(cfg.executable);
    cfg.arguments = expand_env(cfg.arguments);
    for (const auto& arg : wrapper_args.target_args) {
        if (!cfg.arguments.empty()) cfg.arguments += L" ";
        cfg.arguments += quote_windows_arg(arg);
    }

    if (cfg.executable.front() != L'"' && cfg.executable.find(L' ') != std::wstring::npos) {
        cfg.executable = L"\"" + cfg.executable + L"\"";
    }

    std::wstring working_dir = cfg.working_dir.empty()
        ? L""
        : expand_env(cfg.working_dir);
    return launch_process(cfg.executable, cfg.arguments, working_dir, cfg.verbose);
}
