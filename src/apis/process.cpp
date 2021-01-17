#include "alias.hpp"
auto Alias::NewProcess(PseudoConsole* console, std::wstring command_line) noexcept(false) -> Alias::Process* {
    auto startupInfo = CreateStartupInfoForConsole(console);
    PROCESS_INFORMATION piClient{};

    if(CreateProcessW(
       // cmd.data(),                           // No module name - use Command
       // Line
       nullptr,
       (wchar_t*)command_line.data(), // Command Line
       nullptr, // Process handle not inheritable
       nullptr, // Thread handle not inheritable
       FALSE, // Inherit handles
       EXTENDED_STARTUPINFO_PRESENT, // Creation flags
       nullptr, // Use parent's environment block
       nullptr, // Use parent's starting directory
       &(startupInfo.StartupInfo), // Pointer to STARTUPINFO
       &piClient) == 0) // Pointer to PROCESS_INFORMATION
    {
        check_and_throw_error(std::string{"Couldn't create process "} + (char*)command_line.c_str());
    };

    auto process = new Alias::Process{startupInfo, piClient};
    console->process_attached(process);
    return process;
}

auto Alias::Process::stopped() const -> bool {
    DWORD exit_code = 0;
    GetExitCodeProcess(this->process_info.hProcess, &exit_code);
    return exit_code != STILL_ACTIVE;
}