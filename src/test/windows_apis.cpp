#include "catch.hpp" 
#include "apis/alias.hpp"
#include <windows.h>
#include <thread>
CATCH_TRANSLATE_EXCEPTION( Alias::WindowsError const &ex ) {
    return ex.message;
}
namespace CM = Catch::Matchers;
TEST_CASE("Windows API"){
    try{
            Alias::SetupConsoleHost();
    }catch(Alias::WindowsError &ex){
            INFO("Setting up console failed.\
            Continuing anyway as test and debug consoles tend to fail setup.\
            Ensure the console can accept VT sequences or you will get escape codes.");
    }
    Alias::CheckStdOut("\x1b[?1049h"); // Use the alternate buffer so output is clean
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
    SECTION("Creation of process api"){
        auto pseudo_console = Alias::CreatePseudoConsole(0, 0, 130, 20);
        auto win_process = Alias::NewProcess(pseudo_console.get(), L"ping -4 -n 1 localhost");

        REQUIRE(pseudo_console != nullptr);
        REQUIRE(win_process != nullptr);
        REQUIRE(win_process->startup_info.StartupInfo.cb != 0);
        REQUIRE(win_process->process_info.dwProcessId > 0);

        win_process->wait_for_stop(1000);
        pseudo_console->read_output();
        auto output = pseudo_console->latest_output();
        REQUIRE(output.size() > 0);
        REQUIRE(output.find("127.0.0.1") != std::string::npos);
    }
    SECTION("Moving cursor position"){
        auto pseudo_console = Alias::CreatePseudoConsole(15, 20, 130, 100);
        auto win_process = Alias::NewProcess(pseudo_console.get(), L"ping -4 -n 1 google.com");

        win_process->wait_for_stop(1000);
        pseudo_console->read_output();
        auto output = pseudo_console->get_output_buffer()->at(0);
        REQUIRE(output.find("\x1b[20;15H") != std::string::npos);
        std::cout << output.data() << std::endl;

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
        auto bytes_in_pipe = pseudo_console->bytes_in_read_pipe();
        bool looped_entered = false;

        while(!win_process->stopped() || bytes_in_pipe > 0){
            // The ReadFile function that backs the API blocks when no data is avaliable
            // and pipes can't have a timeout assiocated with them for REASONS (SetCommTimeout just throws error)
            if(pseudo_console->bytes_in_read_pipe() > 0){
                auto output = pseudo_console->read_output();
                CHECK((*output).size() > 0);
                if(looped_entered == false){
                    looped_entered = true;
                }
                // auto written = pseudo_console->write(*output.get(), GetStdHandle(STD_OUTPUT_HANDLE));
            }
        }
        REQUIRE(looped_entered); // ensure the looped actually got entered
    }
    Alias::CheckStdOut("\x1b[?1049l"); // switch back to primary buffer
}
TEST_CASE("Win Input"){
    SECTION("Read input"){
        
        auto pseudo_console = Alias::CreatePseudoConsole(0, 0, 130, 100);
        auto win_process = Alias::NewProcess(pseudo_console.get(), L"F:\\dev\\bin\\pswh\\pwsh.exe -nop");

        win_process->wait_for_stop(500); // Use this to ensure the process actually starts
        pseudo_console->read_output();
        REQUIRE_THAT(pseudo_console->latest_output(), CM::Contains("PS"));

        std::string input{"echo Hello\n"};
        pseudo_console->write_input(input);
        win_process->wait_for_stop(500); // Use this to ensure the input goes through

        REQUIRE(pseudo_console->bytes_in_read_pipe() > 0); // Use this to abort the test if the read is going to hang
        pseudo_console->read_output();

        REQUIRE_THAT(pseudo_console->latest_output(), CM::Contains("Hello"));

    }
}
TEST_CASE("Primary console"){
    // Re-bind std in so we can write to it
    HANDLE write_pipe;
    HANDLE read_pipe;
    CreatePipe(&read_pipe, &write_pipe, nullptr, 0);
    SetStdHandle(STD_INPUT_HANDLE, read_pipe);

    SECTION(""){

        std::string input{"hello\n"};
        WriteFile(write_pipe, input.data(), input.size(), nullptr, nullptr);

        Alias::PrimaryConsole console;
        auto read_input = console.read_input_from_console();
        REQUIRE_THAT(read_input, CM::Contains("hello"));
    }
}
TEST_CASE("Code test"){
    SECTION("String splits when ending with new line"){
        namespace CM = Catch::Matchers;
        std::string chars{"abc\ndef\nghi\n"};
        std::vector<std::string> output_buffer;
        char new_line = '\n';

		std::string_view string_to_split{chars.data()};
        Alias::split_string(string_to_split, new_line, &output_buffer);

        REQUIRE(output_buffer.size() == 3);
        REQUIRE_THAT(output_buffer, CM::Equals(std::vector<std::string>{"abc\n", "def\n", "ghi\n"}));

    }
    SECTION("String splits without ending with a new line") {
        namespace CM = Catch::Matchers;
        std::string chars{ "abc\ndef\nghi" };
        std::vector<std::string> output_buffer;
        char new_line = '\n';

        std::string_view string_to_split{ chars.data() };
        Alias::split_string(string_to_split, new_line, &output_buffer);

        REQUIRE(output_buffer.size() == 3);
        REQUIRE_THAT(output_buffer, CM::Equals(std::vector<std::string>{"abc\n", "def\n", "ghi"}));

    }
}
#include <fstream>
#include <io.h>
#include <fcntl.h>
TEST_CASE("idk"){
    HANDLE write_pipe;
    HANDLE read_pipe;
    CreatePipe(&read_pipe, &write_pipe, nullptr, 0);
    // SetStdHandle(STD_INPUT_HANDLE, read_pipe);
    // SetStdHandle(STD_OUTPUT_HANDLE, write_pipe);
    int file_descriptor = _open_osfhandle((intptr_t)GetStdHandle(STD_OUTPUT_HANDLE), _O_APPEND);
    REQUIRE(file_descriptor != -1);

    if (file_descriptor != -1) {
        FILE* file = _fdopen(file_descriptor, "w");
        REQUIRE(file != nullptr);
        std::ofstream stream(file);
        stream << "hello" << "\n";
        stream.flush();
        stream.close();
    }
}