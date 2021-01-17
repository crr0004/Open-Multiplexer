#include "catch.hpp"
#include "omux/console.hpp"
#include <memory>
#include <exception>


namespace CM = Catch::Matchers;
TEST_CASE("Console API"){
    using namespace omux;
    try{
        SetupConsoleHost();
    }catch(std::logic_error &ex){
        // std::cerr << ex.what() << std::endl;
    }

    SECTION("Creation of console and api"){
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 50, 20});
        auto console_two = std::make_shared<Console>(primary_console, Layout{50, 0, 50, 20});
        primary_console->set_active(console_one);
        
        {
            Process ping{console_one, L"F:\\dev\\bin\\pswh\\pwsh.exe", L" -nop -c \"& {1..20 | % {write-host $(1..$_)}}\""};
            Process ping_2{console_two, L"F:\\dev\\bin\\pswh\\pwsh.exe", L" -nop -c \"& {1..20 | % {write-host $(1..$_)}}\""};
        }
        auto output = console_one->output();
        REQUIRE(output.find("1 2") != std::string::npos);
    }

    SECTION("Writing to stdout"){
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 30, 20});
        Process ping{console_one, L"ping", L" localhost -4 -n 1"};
        ping.wait_for_stop(1000);
    }

    SECTION("Active console get input"){
        // This will won't explicity fail, but cause a hang if these things are broken
        // TODO wrap this in a future or something
        std::string input_to_stdin{"exit\r"};
        auto stdin_stream = Alias::Get_StdIn_As_Stream();

        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{ 0, 0, 40, 58 });
        Process ping{console_one, L"F:\\dev\\bin\\pswh\\pwsh.exe", L" -nop"};
        primary_console->set_active(console_one);

        stdin_stream.second << input_to_stdin;
        stdin_stream.second.flush();
        
        primary_console->join_read_thread();

    }
    SECTION("PrimaryConsole waits for all processes to stop"){
        std::string input_to_stdin{"exit\r"};
        std::string output_from_stdin;

        // TODO wrap this in a future or something
        auto stdin_stream = Alias::Get_StdIn_As_Stream();
        
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 40, 20});
        auto console_two = std::make_shared<Console>(primary_console, Layout{45, 0, 130, 20});
        
        Process pwsh{console_one, L"F:\\dev\\bin\\pswh\\pwsh.exe", L" -nop"};
        Process pwsh_2{console_two, L"F:\\dev\\bin\\pswh\\pwsh.exe", L" -nop"};
        primary_console->set_active(console_one);
              
        stdin_stream.second << input_to_stdin;
        stdin_stream.second.flush();
        pwsh.wait_for_stop(1000);

        primary_console->set_active(console_two);
        stdin_stream.second << input_to_stdin;
        stdin_stream.second.flush();

        primary_console->join_read_thread();

    }
    ReverseSetupConsoleHost();
}