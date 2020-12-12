#include "omux/console.hpp"
#include "apis/alias.hpp"

namespace omux{
    auto SetupConsoleHost() noexcept(false) -> bool{
        return Alias::SetupConsoleHost();
    }
    auto ReverseSetupConsoleHost() noexcept(false) -> bool {
        return Alias::ReverseSetupConsoleHost();
    }
    void WriteToStdOut(std::string message){
        Alias::CheckStdOut(message);
    }

    Console::Console(std::shared_ptr<PrimaryConsole> primary_console, Layout layout) : layout(layout), primary_console(primary_console){
        this->pseudo_console = Alias::CreatePseudoConsole(layout.x, layout.y, layout.width, layout.height);
        primary_console->add_console(this);

    }
    Console::Console(std::shared_ptr<PrimaryConsole> primary_console, Layout layout, Console* console) : layout(layout), primary_console(primary_console){
        primary_console->add_console(this);
    }
    Console::~Console() {
        if (running_process != nullptr) {
            primary_console->remove_console(this);
        }
    }
    auto Console::output_at(size_t index) -> std::string{
        return pseudo_console->get_output_buffer()->at(index);

    }
    auto Console::output() -> std::string{
        return pseudo_console->latest_output();

    }
    void Console::process_attached(Process* process){
        this->running_process = process;
    }
    void Console::process_dettached(Process* process) {
        this->running_process = nullptr;
        primary_console->remove_console(this);
    }
    auto Console::is_running() -> bool{
        if(running_process != nullptr){
            return running_process->process_running();
        }
        return false;
       
    }
    std::shared_ptr<PrimaryConsole> Console::get_primary_console() {
        return primary_console;
    }
}