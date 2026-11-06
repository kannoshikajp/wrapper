#pragma once

#include <string>
#include <vector>

struct WrapperArguments {
    std::wstring custom_ini;
    std::vector<std::wstring> target_args;
    bool force_verbose = false;
    bool help_requested = false;
};

bool parse_wrapper_arguments(int argc, wchar_t* argv[], WrapperArguments& result);
std::wstring quote_windows_arg(const std::wstring& arg);
