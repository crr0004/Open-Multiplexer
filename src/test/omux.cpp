#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include "omux/console.hpp"
#include <memory>
#include <exception>

TEST_CASE("Console API"){
    using namespace omux;
    WriteToStdOut(L"\x1b[?1049h"); // Use the alternate buffer so output is clean
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
        REQUIRE(output.find(L"1 2") != std::wstring::npos);
    }
    SECTION("Writing to stdout"){
        auto console_one = std::make_shared<Console>(Layout{0, 0, 30, 20});
        Process ping{console_one, L"ping", L" localhost -4 -n 1"};
        ping.wait_for_stop(1000);
    }
    /*
    SECTION("Active console get input"){
        PrimaryConsole primary_console;
        auto console_one = std::make_shared<Console>(Layout{5, 0, 50, 58});
        Process ping{console_one, L"F:\\dev\\bin\\pswh\\pwsh.exe", L" -nop"};

        primary_console.set_active(console_one);


    }
    */
    WriteToStdOut(L"\x1b[?1049l");
}