#include "catch.hpp"
#include "omux/console.hpp"
#include "omux/actions.hpp"
#include <exception>
#include <memory>



namespace CM = Catch::Matchers;
TEST_CASE("Keybinds") {
    using namespace omux;
    try {
        Alias::SetupConsoleHost();
    } catch(std::logic_error& ex) {
        // std::cerr << ex.what() << std::endl;
    }
    SECTION("Keybind creates an action in the stack") {
        auto action_factory = std::make_shared<ActionFactory>();
        auto primary_console = std::make_shared<PrimaryConsole>(action_factory);
        auto console = std::make_shared<Console>(primary_console, Layout{0, 0, 30, 20});
        
        primary_console->set_active(console.get());
        // Ctrl-<key> is <key code>-64, or with the 6 bit turned off
        // A (0x41) becomes 0x1
        primary_console->process_input("\x1\x23");
        REQUIRE_FALSE(action_factory->get_action_stack()->empty());
        REQUIRE(action_factory->get_action_stack()->back()->get_enum() == Actions::split_vert);
        primary_console->remove_console(console.get());
        primary_console->remove_console(primary_console->get_active_console());
        primary_console->wait_for_attached_consoles();

    }
    SECTION("Prefix followed by non-keybind removes the prefix") {
        auto action_factory = std::make_shared<ActionFactory>();
        auto primary_console = std::make_shared<PrimaryConsole>(action_factory);
        // Ctrl-<key> is <key code>-64, or with the 6 bit turned off
        // A (0x41) becomes 0x1
        primary_console->process_input("\x1");
        REQUIRE_FALSE(action_factory->get_action_stack()->empty());
        REQUIRE(action_factory->get_action_stack()->back()->get_enum() == Actions::prefix);
        primary_console->process_input("a");
        REQUIRE(action_factory->get_action_stack()->empty());
    }
    SECTION("Processed keys are removed") {
        auto action_factory = std::make_shared<ActionFactory>();
        auto primary_console = std::make_shared<PrimaryConsole>(action_factory);
        // Ctrl-<key> is <key code>-64, or with the 6 bit turned off
        // A (0x41) becomes 0x1
        std::string input{"\x1"};
        input = primary_console->process_input(input);
        REQUIRE(input.empty());
        
    }
    SECTION("Normal keys are outputed") {
        auto action_factory = std::make_shared<ActionFactory>();
        auto primary_console = std::make_shared<PrimaryConsole>(action_factory);
        // Ctrl-<key> is <key code>-64, or with the 6 bit turned off
        // A (0x41) becomes 0x1
        std::string input{"a"};
        primary_console->process_input(input);
        REQUIRE(input == "a");

        primary_console->process_input("");
    }
    Alias::ReverseSetupConsoleHost();
}
