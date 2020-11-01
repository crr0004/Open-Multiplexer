#include <iostream>
#include "omux/console.hpp"

int main() {
    using namespace omux;
    auto console_one = std::make_shared<Console>(Layout{0, 0, 30, 20});
    
    Process ping{console_one, L"ping", L" localhost -4 -n 1 -l 8"};
    

}
