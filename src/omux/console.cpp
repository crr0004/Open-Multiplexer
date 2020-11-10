#include "omux/console.hpp"
#include "apis/alias.hpp"

namespace omux{
    bool SetupConsoleHost() noexcept(false){
        return Alias::SetupConsoleHost();
    }
    void WriteToStdOut(std::string message){
        Alias::CheckStdOut(message);
    }

    Console::Console(Layout layout) : layout(layout){
        this->pseudo_console = Alias::CreatePseudoConsole(layout.x, layout.y, layout.width, layout.height);

    }
    Console::Console(Layout layout, Console* console) : layout(layout){

    }
    std::string Console::output_at(size_t index){
        return pseudo_console->get_output_buffer()->at(index);

    }
    std::string Console::output(){
        return pseudo_console->latest_output();

    }
}