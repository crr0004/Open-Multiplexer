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
    Alias::CheckStdOut(L"\x1b[?1049h"); // Use the alternate buffer so output is clean
    SECTION("Ensure we can write to STDOUT"){
        REQUIRE(Alias::CheckStdOut(L"\0"));
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
        REQUIRE(output.find(L"127.0.0.1") != std::wstring::npos);
    }
    SECTION("Moving cursor position"){
        auto pseudo_console = Alias::CreatePseudoConsole(15, 20, 130, 100);
        auto win_process = Alias::NewProcess(pseudo_console.get(), L"ping -4 -n 1 google.com");

        win_process->wait_for_stop(1000);
        pseudo_console->read_output();
        auto output = pseudo_console->get_output_buffer()->at(0);
        REQUIRE(output.find(L"\x1b[20;15H") != std::wstring::npos);
        //std::wcout << output.data() << std::endl;

        //pseudo_console->read_output();
        auto buffer = pseudo_console->get_output_buffer();
        for(auto read_line : *buffer){
            auto written = pseudo_console->write(read_line, GetStdHandle(STD_OUTPUT_HANDLE));
            REQUIRE(written > 0);
        }
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
    Alias::CheckStdOut(L"\x1b[?1049l"); // switch back to primary buffer
}
TEST_CASE("Win Input"){
    try {
        Alias::SetupConsoleHost();
    }
    catch (Alias::WindowsError& ex) {
        INFO("Setting up console failed.\
            Continuing anyway as test and debug consoles tend to fail setup.\
            Ensure the console can accept VT sequences or you will get escape codes.");
    }
    Alias::CheckStdOut(L"\x1b[?1049h"); // Use the alternate buffer so output is clean
    SECTION("echo input from powershell"){
        
        auto pseudo_console = Alias::CreatePseudoConsole(0, 0, 130, 100);
        auto win_process = Alias::NewProcess(pseudo_console.get(), L"F:\\dev\\bin\\pswh\\pwsh.exe -nop");
        std::wstring input{ L"echo Hello\n" };

        win_process->wait_for_stop(1000); // Use this to ensure the process actually starts
        pseudo_console->read_output();
        auto buffer = pseudo_console->get_output_buffer();
        for (auto read_line : *buffer) {
            auto written = pseudo_console->write(read_line, GetStdHandle(STD_OUTPUT_HANDLE));
            REQUIRE(written > 0);
        }
        REQUIRE(pseudo_console->latest_output().find(L"PS") != std::wstring::npos);

        
        pseudo_console->write_input(input);
        win_process->wait_for_stop(500); // Use this to ensure the input goes through

        REQUIRE(pseudo_console->bytes_in_read_pipe() > 0); // Use this to abort the test if the read is going to hang
        pseudo_console->read_output();

        REQUIRE(pseudo_console->latest_output().find(L"Hello") != std::wstring::npos);

    }
    Alias::CheckStdOut(L"\x1b[?1049l"); // switch back to primary buffer
}
TEST_CASE("Primary console"){
    // Re-bind std in so we can write to it
    HANDLE write_pipe;
    HANDLE read_pipe;
    HANDLE old_std_in {GetStdHandle(STD_INPUT_HANDLE)};

    CreatePipe(&read_pipe, &write_pipe, nullptr, 0);
    SetStdHandle(STD_INPUT_HANDLE, read_pipe);

    SECTION("Read input from primary console"){

        std::wstring input{L"hello\n"};
        WriteFile(write_pipe, input.data(), input.size()*sizeof(wchar_t), nullptr, nullptr);

        Alias::PrimaryConsole console;
        auto read_input = console.read_input_from_console();
        REQUIRE(read_input.find(L"hello\n") != std::wstring::npos);
    }
    /*
    SECTION("How is signal stop propagated"){
        try{
            Alias::SetupConsoleHost();
        }catch(Alias::WindowsError &ex){
            INFO("Setting up console failed.\
            Continuing anyway as test and debug consoles tend to fail setup.\
            Ensure the console can accept VT sequences or you will get escape codes.");
        }
        SetStdHandle(STD_INPUT_HANDLE, old_std_in);
        Alias::PrimaryConsole console;
        auto read_input = console.read_input_from_console();

        auto pseudo_console = Alias::CreatePseudoConsole(0, 0, 130, 100);
        pseudo_console->write_to_stdout(read_input);
    }
    */
    SetStdHandle(STD_INPUT_HANDLE, old_std_in);
}

TEST_CASE("Code test"){
    SECTION("String splits when ending with new line"){
        namespace CM = Catch::Matchers;
        std::wstring chars{L"abc\ndef\nghi\n"};
        std::vector<std::wstring> output_buffer;
        char new_line = '\n';

		std::wstring_view string_to_split{chars.data()};
        Alias::Split_String(string_to_split, new_line, &output_buffer);

        REQUIRE(output_buffer.size() == 3);
        REQUIRE_THAT(output_buffer, CM::Equals(std::vector<std::wstring>{L"abc\n", L"def\n", L"ghi\n"}));

    }
    SECTION("String splits without ending with a new line") {
        namespace CM = Catch::Matchers;
        std::wstring chars{ L"abc\ndef\nghi" };
        std::vector<std::wstring> output_buffer;
        char new_line = '\n';

        std::wstring_view string_to_split{ chars.data() };
        Alias::Split_String(string_to_split, new_line, &output_buffer);

        REQUIRE(output_buffer.size() == 3);
        REQUIRE_THAT(output_buffer, CM::Equals(std::vector<std::wstring>{L"abc\n", L"def\n", L"ghi"}));

    }
    SECTION("Re-binding stdout and stdin to fstreams"){
        // This will throw an exception on failure
        auto std_in_out = Alias::Rebind_Std_In_Out();

        // Just check we can write to the streams even though they go nowhere
        std_in_out.second << "hello";

        std_in_out.first << "hello";
    }
}