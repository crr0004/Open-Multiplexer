#include "catch.hpp"
#include "omux/console.hpp"
#include <string_view>

namespace CM = Catch::Matchers;
TEST_CASE("Process output handling") {
    using namespace omux;
    try {
        SetupConsoleHost();
    }
    catch (std::logic_error& ex) {
        // std::cerr << ex.what() << std::endl;
    }
    std::vector<std::string> output_buffer{
            "a\n",
            "b\n",
            "c\n",
            "1\n",
            "2\n",
            "3\n"
    };

    auto stdin_stream = Alias::Get_StdIn_As_Stream();
    auto stdout_stream = Alias::Get_StdOut_As_Stream();
    auto primary_console = std::make_shared<PrimaryConsole>();
    
    // While the console will be attached during the creation of console_one,
    // we don't actually want it to be attached as it will block the tear down
    SECTION("Process handles normal output") {
        auto console_one = std::make_shared<Console>(
            primary_console,
            Layout{ 0, 0, 40, 8 }
        );
        primary_console->remove_console(console_one.get());

        Process pwsh{ console_one };

        

    }
    SECTION("Process handles slight output overflow") {
        auto console_one = std::make_shared<Console>(
            primary_console,
            Layout{ 0, 0, 40, 3 }
        );
        primary_console->remove_console(console_one.get());
        
        Process pwsh{ console_one };

        
        
    }
    SECTION("Process handles complete output overflow") {
        constexpr auto output_height = 3;
        output_buffer.push_back("7\n");
        output_buffer.push_back("8\n");
        output_buffer.push_back("9\n");
        output_buffer.push_back("10\n");
        output_buffer.push_back("11\n");

        auto console_one = std::make_shared<Console>(
            primary_console,
            Layout{ 0, 0, 40, output_height }
        );
        primary_console->remove_console(console_one.get());

        Process pwsh{ console_one };

        
    }
    primary_console->join_read_thread();
    ReverseSetupConsoleHost();
}