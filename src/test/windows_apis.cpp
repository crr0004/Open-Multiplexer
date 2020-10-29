#include "catch.hpp"
#include "apis/alias.hpp"
CATCH_TRANSLATE_EXCEPTION( Alias::WindowsError const &ex ) {
    return ex.message;
}
TEST_CASE("Windows API alias functions"){
    SECTION("Ensure we can write to STDOUT"){
        REQUIRE(Alias::CheckStdOut("\0"));
    }
    SECTION("Creation of Pseudo Console"){
        auto pseudo_console = Alias::CreatePseudoConsole();

        REQUIRE(pseudo_console != nullptr);
        REQUIRE(pseudo_console->pipe_in != nullptr);
        REQUIRE(pseudo_console->pipe_out != nullptr);
        REQUIRE(pseudo_console->pseudo_console_handle != nullptr);
    }
    SECTION("Creation of Process API", "process"){
        // try{
        //     Alias::SetupConsoleHost();
        // }catch(Alias::WindowsError &ex){
        //     INFO("Setting up console failed.\
        //     Continuing anyway as test and debug consoles tend to fail setup.\
        //     Ensure the console can accept VT sequences or you will get escape codes.");
        // }
        auto pseudo_console = Alias::CreatePseudoConsole();
        auto win_process = Alias::NewProcess(pseudo_console.get(), L"ping -4 -n 1 localhost");

        REQUIRE(pseudo_console != nullptr);
        REQUIRE(win_process != nullptr);
        REQUIRE(win_process->startup_info.StartupInfo.cb != 0);
        REQUIRE(win_process->process_info.dwProcessId > 0);

        auto output = pseudo_console->read_output();
        win_process->wait_for_stop(1000);
        REQUIRE(output.size() > 0);
        REQUIRE(output.find("ping.exe") != std::string::npos);

        REQUIRE(output.find("127.0.0.1") != std::string::npos);
        win_process->kill(100);
    }
}