#include "catch.hpp" 
#include "apis/alias.hpp"
#include <windows.h>
#include <thread>
#include <chrono>
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

    SECTION("Ensure we can write to STDOUT"){
        REQUIRE(Alias::WriteToStdOut("\0"));
    }
    SECTION("Creation of Pseudo Console"){
        auto pseudo_console = Alias::CreatePseudoConsole(0, 0, 30, 20);

        REQUIRE(pseudo_console != nullptr);
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
        pseudo_console->read_output().wait();
        auto output = pseudo_console->latest_output();
        REQUIRE(output.size() > 0);
        REQUIRE(output.find("127.0.0.1") != std::string::npos);
    }
    /*
    * Trying to test some behaviour of the console setup. Will come back to this or test elsewhere
    SECTION("Moving cursor position"){
        auto pseudo_console = Alias::CreatePseudoConsole(15, 20, 130, 100);
        auto win_process = Alias::NewProcess(pseudo_console.get(), L"ping -4 -n 1 google.com");

        win_process->wait_for_stop(1000);
        pseudo_console->read_output();
        auto output = pseudo_console->latest_output();
        REQUIRE(output.find("\x1b[20;15H") != std::string::npos);
        //std::wcout << output.data() << std::endl;

    }
    */
    Alias::WriteToStdOut("\x1b[?1049l"); // switch back to primary buffer
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
    Alias::WriteToStdOut("\x1b[?1049h"); // Use the alternate buffer so output is clean
    SECTION("echo input from powershell"){
        
        auto pseudo_console = Alias::CreatePseudoConsole(0, 0, 130, 100);
        auto win_process = Alias::NewProcess(pseudo_console.get(), L"F:\\dev\\bin\\pswh\\pwsh.exe -nop");
        std::string input{ "echo Hello\n" };

        win_process->wait_for_stop(1000); // Use this to ensure the process actually starts
        auto output = pseudo_console->read_output().get();
        //auto buffer = pseudo_console->get_scroll_buffer();
        REQUIRE(output.find("PS") != std::string::npos);

        
        pseudo_console->write_input(input);
        win_process->wait_for_stop(500); // Use this to ensure the input goes through

        REQUIRE(pseudo_console->bytes_in_read_pipe() > 0); // Use this to abort the test if the read is going to hang
        pseudo_console->read_output();

        REQUIRE(pseudo_console->latest_output().find("Hello") != std::string::npos);

    }
    Alias::WriteToStdOut("\x1b[?1049l"); // switch back to primary buffer
}
TEST_CASE("Primary console"){
    // Re-bind std in so we can write to it
    HANDLE write_pipe;
    HANDLE read_pipe;
    HANDLE old_std_in {GetStdHandle(STD_INPUT_HANDLE)};

    CreatePipe(&read_pipe, &write_pipe, nullptr, 0);
    SetStdHandle(STD_INPUT_HANDLE, read_pipe);

    SECTION("Read input from primary console"){

        std::string input{"he\n"};
        WriteFile(write_pipe, input.data(), input.size()*sizeof(char), nullptr, nullptr);

        Alias::MainConsole console;
        auto read_input = console.read_input_from_console();
        read_input.wait();
        REQUIRE_THAT(read_input.get(), CM::Contains("he\n"));
    }
    SECTION("Read input can be destroyed and not block exit"){
        
        auto now = std::chrono::steady_clock::now();
        Alias::MainConsole console;
        auto read_input = console.read_input_from_console();
        read_input.wait_for(std::chrono::milliseconds(100));
        console.cancel_io();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - now);
        REQUIRE(duration.count() < 500);

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

TEST_CASE("Code test") {
    SECTION("Applying string splits without ending on a new line") {
        std::vector<std::string> expected_lines{"a\n", "b\n", "c"};
        bool function_called_once = false;
        Alias::Apply_On_Split_String("a\nb\nc", '\n', [&](auto line) { 

            // Check the line exists in our expected line breaks
            auto line_in_expected = std::find_if(expected_lines.begin(), expected_lines.end(), 
                [&](auto value) {return value == line;});

            REQUIRE( line_in_expected != expected_lines.end());

            function_called_once = true;
            return;
        });
        REQUIRE(function_called_once);
    }
    SECTION("Applying string splits ending on a new line") {
        std::vector<std::string> expected_lines{"a\n", "b\n", "c\n"};
        bool function_called_once = false;
        Alias::Apply_On_Split_String("a\nb\nc\n", '\n', [&](auto line) {
            // Check the line exists in our expected line breaks
            auto line_in_expected =
            std::find_if(expected_lines.begin(), expected_lines.end(),
                         [&](auto value) { return value == line; });

            REQUIRE( line_in_expected != expected_lines.end());

            function_called_once = true;
            return;
        });
        REQUIRE(function_called_once);
    }
    SECTION("Applying string splits on empty string") {
        bool function_called_once = false;
        Alias::Apply_On_Split_String("", '\n', [&](auto line) {
            function_called_once = true;
            return;
        });
        REQUIRE_FALSE(function_called_once);
    }
}

TEST_CASE("Re-binding stdin to fstreams"){
    auto std_in_out = Alias::Get_StdIn_As_Stream();
    std::string hello{ "hello" };
    std::string stdin_output(hello.size(), '\0');
    SECTION("Can write to stream and read through win api") {
            

        // Just check we can write to the streams even though they go nowhere
        std_in_out.second << hello;// << std::endl;
        std_in_out.second.flush();

        HANDLE std_in{ GetStdHandle(STD_INPUT_HANDLE) };
        DWORD bytes_read = 0;
            
        ReadFile(std_in, stdin_output.data(), stdin_output.size(), &bytes_read, nullptr);
        REQUIRE(stdin_output == hello);
    }
    SECTION("Can write and read from streams") {
        std_in_out.second << hello;// << std::endl;
        std_in_out.second.flush();

        std_in_out.first.read(stdin_output.data(), stdin_output.size());
        REQUIRE(stdin_output == hello);           
    }
    std_in_out.first.close();
    std_in_out.second.close();
    //std_in_out.first >> output_from_stream;
}

TEST_CASE("Interrupting read file calls") {
    auto pseudo_console = Alias::CreatePseudoConsole(0, 0, 130, 20);
    
    auto read_future = pseudo_console->read_output();
    read_future.wait_for(std::chrono::microseconds(10));
    pseudo_console->close_pipes();
    auto wait_result = read_future.wait_for(std::chrono::milliseconds(5000));
    REQUIRE(wait_result != std::future_status::timeout);
}