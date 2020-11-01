#include "omux/console.hpp"
#include "apis/alias.hpp"

namespace omux{
    Console::Console(Layout layout){
        this->pseudo_console = Alias::CreatePseudoConsole(layout.x, layout.y, layout.width, layout.height);

    }
    Console::Console(Layout layout, Console* console){

    }
    std::shared_ptr<std::string> Console::output(){
        pseudo_console->read_output();
        return pseudo_console->latest_output();

    }
}