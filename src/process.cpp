#include "process.h"

#include <windows.h>
#include <iostream>

int launch_process(const std::wstring& executable,
                   const std::wstring& arguments,
                   const std::wstring& working_dir,
                   bool verbose) {
    std::wstring cmd_line = executable;
    if (!arguments.empty()) cmd_line += L" " + arguments;

    const wchar_t* work_dir_ptr = working_dir.empty() ? nullptr : working_dir.c_str();
    if (verbose) {
        std::wcout << L"[wrapper:run] Executable: " << executable << L"\n"
                   << L"[wrapper:run] Command line: " << cmd_line << L"\n"
                   << L"[wrapper:run] Working dir: "
                   << (work_dir_ptr ? work_dir_ptr : L"(inherited)") << L"\n";
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    SetConsoleCtrlHandler(NULL, TRUE);
    BOOL success = CreateProcessW(
        NULL, cmd_line.data(), NULL, NULL, TRUE, 0, NULL, work_dir_ptr, &si, &pi);
    if (!success) {
        DWORD err = GetLastError();
        wchar_t* msg = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&msg), 0, NULL);
        std::wcerr << L"Error: Failed to launch '" << executable << L"': "
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
