#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include "omux/console.hpp"
#include <memory>

TEST_CASE("Console API"){
    SECTION("Creation of console and api"){
        using namespace omux;
        auto console_one = std::make_shared<Console>(Layout{0.5, 1.0});
        
        Process ping{console_one, L"ping", L" localhost -4 -n 1 -l 8"};

        ping.wait_for_stop(1000);
        auto output = console_one->output();
        REQUIRE(output.find("ping.exe") != std::string::npos);
    
        REQUIRE(output.find("127.0.0.1") != std::string::npos);
    }
}