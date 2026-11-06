#include "arguments.h"

#include <iostream>
#include <utility>

std::wstring quote_windows_arg(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;

    std::wstring quoted = L"\"";
    size_t backslashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            ++backslashes;
        } else if (c == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted += L'"';
            backslashes = 0;
        } else {
            quoted.append(backslashes, L'\\');
            quoted += c;
            backslashes = 0;
        }
    }
    quoted.append(backslashes * 2, L'\\');
    quoted += L'"';
    return quoted;
}

bool parse_wrapper_arguments(int argc, wchar_t* argv[], WrapperArguments& result) {
    bool forward_all = false;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (!forward_all && arg == L"--") {
            forward_all = true;
        } else if (!forward_all && (arg == L"--config" || arg == L"-c")) {
            if (i + 1 >= argc) {
                std::wcerr << L"Error: --config requires a file path argument.\n";
                return false;
            }
            result.custom_ini = argv[++i];
        } else if (!forward_all && (arg == L"--verbose" || arg == L"-v")) {
            result.force_verbose = true;
        } else if (!forward_all && (arg == L"--help" || arg == L"-h")) {
            result.help_requested = true;
            std::wcout << L"Usage: wrapper.exe [options] [target_args...]\n"
                       << L"Options:\n"
                       << L"  -c, --config <file>  Specify path to config.ini\n"
                       << L"  -v, --verbose        Print wrapper diagnostics\n"
                       << L"  -h, --help           Show this help message\n"
                       << L"  --                   Stop parsing wrapper options\n";
            return false;
        } else {
            result.target_args.push_back(std::move(arg));
        }
    }
    return true;
}
