#include "omux/console.hpp"

namespace omux{
    PrimaryConsole::PrimaryConsole(){

    }
    void PrimaryConsole::set_active(Console::Sptr console){
        this->active_console = console;
    }
}