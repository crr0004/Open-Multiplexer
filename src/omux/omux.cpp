#include <iostream>
#include "omux/console.hpp"

int main() {
    using namespace omux;
    try {
        SetupConsoleHost();
    }catch(std::logic_error &ex){}
    // WriteToStdOut("\x1b[?1049h");
    auto console_one = std::make_shared<Console>(Layout{0, 0, 30, 20});
    auto console_two = std::make_shared<Console>(Layout{35, 0, 30, 20 });
    
    Process pwsh{ console_one, L"F:\\dev\\bin\\pswh\\pwsh.exe", L" -nop -c \"& {1..5 | % {write-host $(1..$_)}}\"" };
    
    Process pwsh_2{ console_two, L"F:\\dev\\bin\\pswh\\pwsh.exe", L" -nop -c \"& {1..5 | % {write-host $(1..$_)}}\"" };
}
