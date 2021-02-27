#include "catch.hpp"
#include "omux/console.hpp"
#include <string_view>
#include <ranges>
#include <gmock\gmock.h>

namespace CM = Catch::Matchers;
using namespace omux;
namespace gmock = ::testing;

class PrimaryConsoleMock : public PrimaryConsole {
    public:
    MOCK_METHOD(void, write_to_stdout, (std::string_view in), (override));
    MOCK_METHOD(void, write_to_stdout, (std::stringstream& in), (override));
    MOCK_METHOD(bool, write_character_to_stdout, (char output), (override));
};

auto get_primary_console_mock_with_capture(std::stringstream* stdout_capture) {
    auto mock_primary_console = std::make_shared<gmock::NiceMock<PrimaryConsoleMock>>();
    ON_CALL(*mock_primary_console, write_to_stdout(gmock::A<std::string_view>())).WillByDefault([&](std::string_view in) {
        *stdout_capture << in;
        mock_primary_console->PrimaryConsole::write_to_stdout(in);
        return;
    });
    ON_CALL(*mock_primary_console, write_to_stdout(gmock::A<std::stringstream&>())).WillByDefault([&](std::stringstream& in) {
        *stdout_capture << in.str();
        mock_primary_console->PrimaryConsole::write_to_stdout(in);
        return;
    });
    ON_CALL(*mock_primary_console, write_character_to_stdout(gmock::A<char>())).WillByDefault([&](char in) {
        *stdout_capture << in;
        return mock_primary_console->PrimaryConsole::write_character_to_stdout(in);
    });
    return mock_primary_console;
}

TEST_CASE("Process output with real stdout") {
    try {
        Alias::SetupConsoleHost();
    } catch(std::logic_error& ex) {
        // std::cerr << ex.what() << std::endl;
    }
    std::stringstream stdout_capture;
    
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

        primary_console->wait_for_attached_consoles();
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

        primary_console->wait_for_attached_consoles();
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

        primary_console->wait_for_attached_consoles();
    }
    SECTION("Process handles position reset control sequences into the scroll buffer") {
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
        REQUIRE(console_one->get_scroll_buffer()->at(2).compare("\x1b[0;0HP") == 0);

        primary_console->wait_for_attached_consoles();
    }
    SECTION("Process handles a new line when at max height") {
        // These seem to appear from the conhost when the max screen buffer line limit is reached
        
        //auto primary_console = std::make_shared<PrimaryConsole>();
        
        auto mock_primary_console = get_primary_console_mock_with_capture(&stdout_capture);
        auto console_one = std::make_shared<Console>(mock_primary_console, Layout{0, 0, 40, 3});
        mock_primary_console->remove_console(console_one.get());

        Process pwsh{console_one};
        pwsh.process_string_for_output("1\n2\n3");

        pwsh.process_string_for_output("\n4\n");



        mock_primary_console->wait_for_attached_consoles();
       // delete &read_future; // do this because it can often cause lock ups

        // Two lines are in the scroll buffer
        REQUIRE(console_one->get_scroll_buffer()->size() == 5);

       REQUIRE(stdout_capture.str().find("2\n\x1b[0G3\n\x1b[0G4") != std::string::npos);

    }
    
    
    Alias::ReverseSetupConsoleHost();
}
TEST_CASE("Process handles scrollbufer re-writes") {
    using namespace omux;
    try {
        Alias::SetupConsoleHost();
    } catch(std::logic_error& ex) {
        // std::cerr << ex.what() << std::endl;
    }
    SECTION("Control sequences that move the cursor back into the line") {
        // These seem to appear from the conhost when the max screen buffer line limit is reached
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 40, 30});

        Process pwsh{console_one};
        pwsh.process_string_for_output("Hello\x1b[GHello");

        // Two lines are in the scroll buffer
         REQUIRE(console_one->get_scroll_buffer()->size() == 1);
         REQUIRE(console_one->get_scroll_buffer()->at(0).compare("Hello") == 0);

    }
    SECTION("Absolute control sequences that move the cursor back into the line") {
        // These seem to appear from the conhost when the max screen buffer line limit is reached
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 40, 30});

        Process pwsh{console_one};
        pwsh.process_string_for_output("Hello\x1b[0;1HHello");

        // Two lines are in the scroll buffer
        REQUIRE(console_one->get_scroll_buffer()->size() == 1);
        REQUIRE(console_one->get_scroll_buffer()->at(0).compare("Hello") == 0);

    }
    
    
    SECTION("Absolute control sequences that move the cursor up lines") {
        // These seem to appear from the conhost when the max screen buffer line limit is reached
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 40, 30});

        Process pwsh{console_one};
        pwsh.process_string_for_output("\n\nHello\x1b[0;1HHello");

        // Two lines are in the scroll buffer
        REQUIRE(console_one->get_scroll_buffer()->size() == 1);
        REQUIRE(console_one->get_scroll_buffer()->at(0).compare("Hello") == 0);
    }
    SECTION("Deleting characters correctly position in the line") {
        std::string line{
        "\x1b[?25h\x1b[?25lPS F \\dev\\projects\\open_multiplexer\\build\\Clang x64-Debug> "
        "\x1b[?25h\x1b[?25l\x1b[?25h\x1b[97m1\x1b[?25l\x1b[m\x1b[?25h\x1b[?25l\x1b[9712\x1b[?25h\x1b[?25l\x1b["
        "m\x1b[?25h\x1b[?25l\x1b[97m"
        "\x1b[10;60H"};
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 40, 30});

        Process pwsh{console_one};
        line = pwsh.delete_n_renderable_characters_from_string(line, 6);
        REQUIRE(line == "\x1b[?25h\x1b[?25lPS F ");

        line = "\x1b[?25h\x1b[?25lPS\x1b[?25l\x1b[?25l";
        line = pwsh.delete_n_renderable_characters_from_string(line, 4);
        REQUIRE(line == "\x1b[?25h\x1b[?25lPS ");

    }
    Alias::ReverseSetupConsoleHost();
}