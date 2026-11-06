#pragma once

#include <string>

int launch_process(const std::wstring& executable,
                   const std::wstring& arguments,
                   const std::wstring& working_dir,
                   bool verbose);
