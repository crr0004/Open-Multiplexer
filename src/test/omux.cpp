#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include "omux/console.hpp"
#include <memory>
#include <exception>

namespace CM = Catch::Matchers;
TEST_CASE("Console API"){
    using namespace omux;
    WriteToStdOut("\x1b[?1049h"); // Use the alternate buffer so output is clean
    try{
        SetupConsoleHost();
    }catch(std::logic_error &ex){
        // std::cerr << ex.what() << std::endl;
    }

    SECTION("Creation of console and api"){
        auto console_one = std::make_shared<Console>(Layout{5, 0, 50, 58});
        auto console_two = std::make_shared<Console>(Layout{50, 0, 50, 58});
        
        {
            Process ping{console_one, L"F:\\dev\\bin\\pswh\\pwsh.exe", L" -nop -c \"& {1..20 | % {write-host $(1..$_)}}\""};
            Process ping_2{console_two, L"F:\\dev\\bin\\pswh\\pwsh.exe", L" -nop -c \"& {1..20 | % {write-host $(1..$_)}}\""};
        }
        auto output = console_one->output();
        REQUIRE(output.find("1 2") != std::string::npos);
    }

    SECTION("Writing to stdout"){
        auto console_one = std::make_shared<Console>(Layout{0, 0, 30, 20});
        Process ping{console_one, L"ping", L" localhost -4 -n 1"};
        ping.wait_for_stop(1000);
    }

    SECTION("Active console get input"){
        // TODO This is failing because there is a race condition between process
        // writing to stdout and primary console writing to stdin which causes a write to stdin
        std::string input_to_stdin{"echo hello\x0D\x18"};
        std::string output_from_stdin;

        //auto stdin_stream = Alias::Get_StdIn_As_Stream();
       // stdin_stream.second << input_to_stdin << std::endl;
        
       // stdin_stream.first >> output_from_stdin;
       // REQUIRE_THAT(input_to_stdin, CM::Contains(output_from_stdin));
        

        auto console_one = std::make_shared<Console>(Layout{0, 0, 130, 58});
        Process ping{console_one, L"F:\\dev\\bin\\pswh\\pwsh.exe", L" -nop"};

        PrimaryConsole primary_console{ console_one };
        //stdin_stream.second << input_to_stdin;
        //stdin_stream.second.flush();
        //primary_console.set_active(console_one);
        primary_console.join_read_thread();

    }
    WriteToStdOut("\x1b[?1049l");
}