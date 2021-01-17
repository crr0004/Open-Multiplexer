#include "catch.hpp"
#include "omux/console.hpp"
#include <string_view>
#include <ranges>

namespace CM = Catch::Matchers;
TEST_CASE("Process output handling", "[process]") {
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
    
    
    SECTION("Process handles end of output not ending in a newline") {
        auto console_one = std::make_shared<Console>(
            primary_console,
            Layout{ 0, 0, 40, 8 }
        );
        // While the console will be attached during the creation of console_one,
        // we don't actually want it to be attached as it will block the tear down
        primary_console->remove_console(console_one.get());

        Process pwsh{ console_one };
        
        //REQUIRE(pwsh.process_string_for_output("a\nb\nc") == "c");
       // REQUIRE(pwsh.process_string_for_output("e") == "ce");
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
    stdin_stream.first.close();
    stdin_stream.second.close();
    stdout_stream.first.close();
    stdout_stream.second.close();
    ReverseSetupConsoleHost();
}
TEST_CASE("Process output with real stdout", "[process]") {
    using namespace omux;
    try {
        SetupConsoleHost();
    } catch(std::logic_error& ex) {
        // std::cerr << ex.what() << std::endl;
    }
    SECTION("Process handles movement control sequences into the scroll buffer") {
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one =
        std::make_shared<Console>(primary_console, Layout{0, 0, 40, 30});
        primary_console->remove_console(console_one.get());

        Process pwsh{console_one};
        pwsh.process_string_for_output("T\x1b[3;1H\x1b[?25h\x1b[?25l.");
        
        // Two lines are in the scroll buffer
        REQUIRE(console_one->get_scroll_buffer()->size() == 3);
        REQUIRE(console_one->get_scroll_buffer()->at(0).compare("T\n") == 0);
        REQUIRE(console_one->get_scroll_buffer()->at(1).compare("\n") == 0);
        REQUIRE(console_one->get_scroll_buffer()->at(2).compare("\x1b[?25h\x1b[?25l.") == 0);

        primary_console->join_read_thread();
    }
    SECTION("Process handles control sequences as new lines into the scroll buffer") {
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one =
        std::make_shared<Console>(primary_console, Layout{0, 0, 40, 30});
        primary_console->remove_console(console_one.get());

        Process pwsh{console_one};
        pwsh.process_string_for_output("\x1b[3;1H\x1b[?25h\x1b[?25l.");

        // Two lines are in the scroll buffer
        REQUIRE(console_one->get_scroll_buffer()->size() == 3);
        REQUIRE(console_one->get_scroll_buffer()->at(0).compare("\n") == 0);
        REQUIRE(console_one->get_scroll_buffer()->at(1).compare("\n") == 0);
        REQUIRE(console_one->get_scroll_buffer()->at(2).compare(
                "\x1b[?25h\x1b[?25l.") == 0);

        primary_console->join_read_thread();
    }
    SECTION("Process handles control sequences that don't go anywhere into the scroll buffer") {
        // These seem to appear from the conhost when the max screen buffer line limit is reached
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 40, 30});
        primary_console->remove_console(console_one.get());

        Process pwsh{console_one};
        pwsh.process_string_for_output("A\x1b[1;2H\x1b[?25h\x1b[?25l.");

        // Two lines are in the scroll buffer
        REQUIRE(console_one->get_scroll_buffer()->size() == 1);
        REQUIRE(console_one->get_scroll_buffer()->at(0).compare("A\x1b[?25h\x1b[?25l.") == 0);

        primary_console->join_read_thread();
    }
    SECTION("Process handles control sequences that don't go anywhere into the scroll buffer") {
        // These seem to appear from the conhost when the max screen buffer line limit is reached
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 40, 30});
        primary_console->remove_console(console_one.get());

        Process pwsh{console_one};
        pwsh.process_string_for_output("A\n\n\x1b[HP");

        // Two lines are in the scroll buffer
        REQUIRE(console_one->get_scroll_buffer()->size() == 3);
        REQUIRE(console_one->get_scroll_buffer()->at(0).compare("A\n") == 0);
        REQUIRE(console_one->get_scroll_buffer()->at(1).compare("\n") == 0);
        REQUIRE(console_one->get_scroll_buffer()->at(2).compare("P") == 0);

        primary_console->join_read_thread();
    }
    SECTION("Process handles a new line when at max height") {
        // These seem to appear from the conhost when the max screen buffer line limit is reached
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 40, 3});
        primary_console->remove_console(console_one.get());

        Process pwsh{console_one};
        pwsh.process_string_for_output("1\n2\n3");

        auto stdout_stream = Alias::Get_StdOut_As_Stream();
        primary_console->reset_stdout();
        pwsh.process_string_for_output("\n4\n");
        stdout_stream.second.flush();
        stdout_stream.first.sync();
        

        
        std::string stdout_contents;
        {
            std::future_status result = std::future_status::deferred;
            auto read_future = std::async([&]() {
                std::string result{};
                stdout_stream.first >> result;
                return result;
            });

            do {

                result = read_future.wait_for(std::chrono::milliseconds(16));
                if(read_future.valid() && result == std::future_status::ready) {
                    stdout_contents.append(read_future.get());
                    read_future = std::async([&]() {
                        std::string result{};
                        stdout_stream.first >> result;
                        return result;
                    });
                }
            } while(result != std::future_status::timeout);
            // Fix the bullshit future method for flushing stdout above. Please find a better way to do this!
            stdout_stream.second << "\n";
            stdout_stream.second.flush();
        }
        
        
        
        

        primary_console->join_read_thread();
        Alias::Cancel_IO_On_StdOut();
        Alias::Reset_StdHandles_To_Real();
       // delete &read_future; // do this because it can often cause lock ups

        // Two lines are in the scroll buffer
        REQUIRE(console_one->get_scroll_buffer()->size() == 5);

        REQUIRE(stdout_contents.find("2\x1b[0G3\x1b[0G4") != std::string::npos);
        
        

        
        
    }
    
    ReverseSetupConsoleHost();
}