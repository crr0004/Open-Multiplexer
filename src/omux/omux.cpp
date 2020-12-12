#include <iostream>
#include "omux/console.hpp"

auto main() -> int {
    using namespace omux;
    try {
        SetupConsoleHost();
    }catch(std::logic_error &ex){}
    
    WriteToStdOut("\x1b[?1049h");
    auto console = std::make_shared<PrimaryConsole>();
    auto console_one = std::make_shared<Console>(console, Layout{0, 0, 40, 20});
    auto console_two = std::make_shared<Console>(console, Layout{45, 0, 30, 20 });
    
    Process pwsh{ console_one, L"F:\\dev\\bin\\pswh\\pwsh.exe", L"" };
    
    Process pwsh_2{ console_two, L"F:\\dev\\bin\\pswh\\pwsh.exe", L" -nop -c \"& {1..5 | % {write-host $(1..$_)}}\"" };
    
    console->set_active(console_one);
    
   // console.set_active(console_one);
    
    console->join_read_thread();
    ReverseSetupConsoleHost();
}
