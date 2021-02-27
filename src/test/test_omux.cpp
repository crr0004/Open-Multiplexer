#include "catch.hpp"
#include "omux/console.hpp"
#include <memory>
#include <exception>


namespace CM = Catch::Matchers;
TEST_CASE("Console API"){
    using namespace omux;
    try{
        Alias::SetupConsoleHost();
    }catch(std::logic_error &ex){
        // std::cerr << ex.what() << std::endl;
    }
    /*
    * // TODO Refactor this to rebind to stdout and read the output that way
    SECTION("Creation of console and api"){
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 50, 20});
        auto console_two = std::make_shared<Console>(primary_console, Layout{50, 0, 50, 20});
        primary_console->set_active(console_one);
        
        {
            Process ping{console_one, omux::PWSH_CONSOLE_PATH, L" -nop -c \"& {1..20 | % {write-host $(1..$_)}}\""};
            Process ping_2{console_two, omux::PWSH_CONSOLE_PATH, L" -nop -c \"& {1..20 | % {write-host $(1..$_)}}\""};
        }
        //auto output = console_one->output();
        REQUIRE(output.find("1 2") != std::string::npos);
        primary_console->wait_for_attached_consoles();
    }
    */
    
    SECTION("Active console get input") {
        // This will won't explicity fail, but cause a hang if these things are broken
        // TODO wrap this in a future or something

        auto primary_console = std::make_shared<PrimaryConsole>();
        
            auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 80, 58});
            Process pwsh{console_one, omux::PWSH_CONSOLE_PATH, L" -nop"};
            primary_console->set_active(console_one.get());


            primary_console->write_input("exit\r");
        
        primary_console->wait_for_attached_consoles();
    }
    
    SECTION("PrimaryConsole waits for all processes to stop"){
        // This will won't explicity fail, but cause a hang if these things are broken
        // TODO wrap this in a future or something so it doesn't get held up and actually fails      
        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, 40, 20});
        auto console_two = std::make_shared<Console>(primary_console, Layout{45, 0, 130, 20});
        
        Process pwsh{console_one, omux::PWSH_CONSOLE_PATH, L" -nop"};
        Process pwsh_2{console_two, omux::PWSH_CONSOLE_PATH, L" -nop"};
        primary_console->set_active(console_one.get());
        
        //pwsh.wait_for_stop(1000);
        primary_console->write_input("exit\r");
        //pwsh.wait_for_stop(1000);

        primary_console->set_active(console_two.get());
        primary_console->write_input("exit\r");
       // pwsh_2.wait_for_stop(1000);
        
        primary_console->wait_for_attached_consoles();
    }
    
    Alias::ReverseSetupConsoleHost();
}

TEST_CASE("Resizing consoles") {
    using namespace omux;
    try {
        Alias::SetupConsoleHost();
    } catch(std::logic_error& ex) {
        // std::cerr << ex.what() << std::endl;
    }
    SECTION("Resize one console") {
        auto original_width = 40;
        auto original_height = 20;
        auto change_in_width = 20;

        auto primary_console = std::make_shared<PrimaryConsole>();
        auto console_one = std::make_shared<Console>(primary_console, Layout{0, 0, original_width, original_height});
        Process pwsh{console_one, omux::PWSH_CONSOLE_PATH, L" -nop"};

        primary_console->set_active(console_one.get());
        primary_console->write_input("\r\rls\r");
        pwsh.wait_for_stop(1000);

        // Get the last height lines
        std::vector<std::string> original_lines;
        original_lines.insert(
            original_lines.begin(),
            console_one->get_scroll_buffer()->end()-original_height,
            console_one->get_scroll_buffer()->end()
        );

        console_one->resize(Layout{0, 0, original_width - change_in_width, original_height});
        pwsh.wait_for_stop(1000);

        std::vector<std::string> new_lines;
        new_lines.insert(
            new_lines.begin(),
            console_one->get_scroll_buffer()->end() - original_height,
            console_one->get_scroll_buffer()->end()
        );

        // Should now see the affect in the scroll buffer
        REQUIRE(new_lines != original_lines);

        primary_console->write_input("exit\r");
        primary_console->wait_for_attached_consoles();
        
    }
    SECTION("Resizing doesn't impact other consoles") {
    
    }
    SECTION("Resizing correctly affects scroll buffer") {
    
    }
    Alias::ReverseSetupConsoleHost();
}

TEST_CASE("Layout management") {
    using namespace omux;
    try {
        Alias::SetupConsoleHost();
    } catch(std::logic_error& ex) {
        // std::cerr << ex.what() << std::endl;
    }
    SECTION("Vert Split active console") {
        auto primary_console = std::make_shared<PrimaryConsole>();

        auto terminal_size = primary_console->get_terminal_size();
       // terminal_size.height -=2; // Ensure we don't use the last line of the console to prevent autoscrolling
        auto console_one = std::make_shared<Console>(primary_console, terminal_size);
        Process pwsh{console_one, omux::PWSH_CONSOLE_PATH, L" -nop"};
        pwsh.wait_for_stop(1000);

        primary_console->set_active(console_one.get());

        auto console_two = primary_console->split_active_console(VERT);
        Process pwsh_2{console_two, omux::PWSH_CONSOLE_PATH, L" -nop"};
        pwsh_2.wait_for_stop(1000);
        
        primary_console->write_input("exit\r");
        pwsh.wait_for_stop(1000);
        primary_console->set_active(console_two.get());
        primary_console->write_input("exit\r");
        primary_console->wait_for_attached_consoles();
        
        

        REQUIRE(console_one->get_layout().x == 0);
        REQUIRE(console_one->get_layout().y == 0);
        REQUIRE(console_one->get_layout().width == (terminal_size.width/2)-1);
        REQUIRE(console_one->get_layout().height == terminal_size.height);

        REQUIRE(console_two->get_layout().x == (terminal_size.width/2)+1);
        REQUIRE(console_two->get_layout().y == 0);
        REQUIRE(console_two->get_layout().width == console_one->get_layout().width);
        REQUIRE(console_two->get_layout().height == terminal_size.height);
    
    }
    SECTION("Horizontal Split active console") {
        auto primary_console = std::make_shared<PrimaryConsole>();

        auto terminal_size = primary_console->get_terminal_size();
       // terminal_size.height--; // Ensure we don't use the last line of the console to prevent autoscrolling
        auto console_one = std::make_shared<Console>(primary_console, terminal_size);
        Process pwsh{console_one, omux::PWSH_CONSOLE_PATH, L" -nop"};
        pwsh.wait_for_stop(1000);

        primary_console->set_active(console_one.get());

        auto console_two = primary_console->split_active_console(HORI);
        Process pwsh_2{console_two, omux::PWSH_CONSOLE_PATH, L" -nop"};
        pwsh_2.wait_for_stop(1000);

        primary_console->write_input("exit\r");
        pwsh.wait_for_stop(2000);
        primary_console->set_active(console_two.get());
        primary_console->write_input("exit\r");
        primary_console->wait_for_attached_consoles();


        REQUIRE(console_one->get_layout().x == 0);
        REQUIRE(console_one->get_layout().y == 0);
        REQUIRE(console_one->get_layout().width == terminal_size.width);
        REQUIRE(console_one->get_layout().height == (terminal_size.height/2)-1);

        REQUIRE(console_two->get_layout().x == 0);
        REQUIRE(console_two->get_layout().y == console_one->get_layout().height+2);
        REQUIRE(console_two->get_layout().width == console_one->get_layout().width);
        REQUIRE(console_two->get_layout().height == (terminal_size.height / 2) - 1);
    }

    Alias::ReverseSetupConsoleHost();
}