#include "omux/console.hpp"
#include <iostream>

auto main() -> int {
    using namespace omux;
    try {
        Alias::SetupConsoleHost();
    } catch(std::logic_error& ex) {
    }

    auto console = std::make_shared<PrimaryConsole>();
    auto console_one = std::make_shared<Console>(console, Layout{0, 0, 80, 10});
    //auto console_two = std::make_shared<Console>(console, Layout{0, 11, 80, 10 });
    //auto console_three = std::make_shared<Console>(console, Layout{85, 0, 30, 10});

    Process pwsh{console_one, omux::PWSH_CONSOLE_PATH,
                 // L"C:\\Windows\\System32\\cmd.exe",
                 L""};

     //Process pwsh_2{ console_two, omux::PWSH_CONSOLE_PATH, L" -nop -c \"& {ping -t google.com}\"" };
    //Process pwsh_3{console_three, omux::PWSH_CONSOLE_PATH, L" -nop -c \"& {ping -t bing.com}\""};

    console->set_active(console_one.get());
     //console->set_active(console_two);

    // console.set_active(console_one);

    console->wait_for_attached_consoles();
    Alias::ReverseSetupConsoleHost();
}
