#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

struct Config {
    std::wstring executable;
    std::wstring arguments;
    std::wstring working_dir;
    bool verbose = false;
    std::vector<std::pair<std::wstring, std::wstring>> env_vars;
};

std::wstring expand_env(const std::wstring& str);
bool parse_ini(const fs::path& ini_path, Config& cfg);
fs::path get_exe_dir();
fs::path resolve_ini_path(const std::wstring& custom_path, const fs::path& exe_dir);
