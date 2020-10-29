#include "omux/console.hpp"
#include "apis/alias.hpp"
#include <string>

namespace omux{
    Console::Console(Layout layout){
        this->pseudo_console = Alias::CreatePseudoConsole();

    }
    Console::Console(Layout layout, Console* console){

    }
    std::string Console::output(){
        return pseudo_console->read_output();

    }
}