#include "catch.hpp" 
#include "apis/alias.hpp"
#include <windows.h>
#include <thread>
CATCH_TRANSLATE_EXCEPTION( Alias::WindowsError const &ex ) {
    return ex.message;
}
TEST_CASE("Windows API alias functions"){
    try{
            Alias::SetupConsoleHost();
    }catch(Alias::WindowsError &ex){
            INFO("Setting up console failed.\
            Continuing anyway as test and debug consoles tend to fail setup.\
            Ensure the console can accept VT sequences or you will get escape codes.");
    }
    SECTION("Ensure we can write to STDOUT"){
        REQUIRE(Alias::CheckStdOut("\0"));
    }
    SECTION("Creation of Pseudo Console"){
        auto pseudo_console = Alias::CreatePseudoConsole(0, 0, 30, 20);

        REQUIRE(pseudo_console != nullptr);
        REQUIRE(pseudo_console->pipe_in != nullptr);
        REQUIRE(pseudo_console->pipe_out != nullptr);
        REQUIRE(pseudo_console->pseudo_console_handle != nullptr);
    }
    SECTION("Creation of process api", "process"){
        auto pseudo_console = Alias::CreatePseudoConsole(0, 0, 130, 20);
        auto win_process = Alias::NewProcess(pseudo_console.get(), L"ping -4 -n 1 localhost");

        REQUIRE(pseudo_console != nullptr);
        REQUIRE(win_process != nullptr);
        REQUIRE(win_process->startup_info.StartupInfo.cb != 0);
        REQUIRE(win_process->process_info.dwProcessId > 0);

        win_process->wait_for_stop(1000);
        pseudo_console->read_output();
        auto output = pseudo_console->latest_output();
        REQUIRE(output->size() > 0);
        REQUIRE(output->find("ping.exe") != std::string::npos);

        REQUIRE(output->find("127.0.0.1") != std::string::npos);
    }
    SECTION("Moving cursor position"){
        auto pseudo_console = Alias::CreatePseudoConsole(15, 20, 130, 100);
        auto win_process = Alias::NewProcess(pseudo_console.get(), L"ping -4 -n 1 google.com");

        win_process->wait_for_stop(1000);
        auto output = pseudo_console->latest_output();
        REQUIRE(output->find("\x1b[15;20H") != std::string::npos);

    /*
        pseudo_console->read_output();
        auto buffer = pseudo_console->get_output_buffer();
        for(auto read_line : buffer){
            auto written = pseudo_console->write(*read_line.get(), GetStdHandle(STD_OUTPUT_HANDLE));
            REQUIRE(written > 0);
        }
    */
    }
    SECTION("Async writing to stdout"){
        auto pseudo_console = Alias::CreatePseudoConsole(15, 20, 130, 100);
        auto win_process = Alias::NewProcess(pseudo_console.get(), L"ping -4 -n 1 google.com");
        auto write_function = [&](){
                while(!win_process->stopped()){
                    pseudo_console->read_output();
                    auto output = pseudo_console->latest_output();
                    auto written = pseudo_console->write(*output.get(), GetStdHandle(STD_OUTPUT_HANDLE));
                }
            };
        std::thread write_to_stdout(write_function);
        write_to_stdout.join();

    }
}