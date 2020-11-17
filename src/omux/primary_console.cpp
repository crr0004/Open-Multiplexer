#include "omux/console.hpp"
#include <mutex>
#include <thread>

namespace omux{
    /**
     * As the active_console is going to called from a lambda thread
     * and being set from different places, we need to ensure we lock guard it.
     */
    std::mutex active_console_lock;
    std::thread stdin_read_thread;
    PrimaryConsole::PrimaryConsole(){
        stdin_read_thread = std::thread([&](){

        });
        


    }
    void PrimaryConsole::set_active(Console::Sptr console){
        std::scoped_lock lock(active_console_lock);
        this->active_console = console;
    }
}