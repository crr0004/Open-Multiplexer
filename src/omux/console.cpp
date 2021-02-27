#include "omux/console.hpp"
#include "apis/alias.hpp"

using namespace omux;
auto SetupConsoleHost() noexcept(false) -> bool {
    return Alias::SetupConsoleHost();
}
auto ReverseSetupConsoleHost() noexcept(false) -> bool {
    return Alias::ReverseSetupConsoleHost();
}
void WriteToStdOut(std::string message) {
    Alias::WriteToStdOut(message);
}

Console::Console(std::shared_ptr<PrimaryConsole> primary_console, Layout layout)
: layout(layout), running_process(nullptr), primary_console(primary_console) {
    if(layout.width < 1 || layout.height < 1) {
        throw OmuxError("Layout has an invalid width or height, they must both be greater than 0");
    }
    this->pseudo_console = Alias::CreatePseudoConsole(layout.x, layout.y, layout.width, layout.height);
    this->primary_console->add_console(this);
        
}
Console::Console(std::shared_ptr<PrimaryConsole> primary_console, Layout layout, Console* console)
: layout(layout), running_process(nullptr), primary_console(primary_console) {
    if(layout.width < 1 || layout.height < 1) {
        throw OmuxError("Layout has an invalid width or height, they must both be greater than 0");
    }
    this->primary_console->add_console(this);
}
Console::~Console() {
    primary_console->remove_console(this);
    running_process = nullptr;
}
auto Console::output_at(size_t index) -> std::string_view {
    return scroll_buffer.at(index);
}
auto Console::get_scroll_buffer() -> std::vector<std::string>* {
    return &scroll_buffer;
}
auto Console::output() -> std::string {
    return pseudo_console->latest_output();
}
void Console::process_attached(Process* process) {
    this->running_process = process;
    first_process_added = true;
}
void Console::process_dettached(Process* process) {
    // Just really be sure this is the process that was attached
    if(running_process == process) {
        this->running_process = nullptr;
    }    
    //primary_console->remove_console(this);
}
auto Console::is_running() -> bool {
    if(running_process) {
        //return running_process->process_running();
    }
    return !first_process_added || running_process;
}
std::shared_ptr<PrimaryConsole> Console::get_primary_console() {
    return primary_console;
}
auto Console::get_saved_cursor() -> std::string {
    if(running_process) {
        return this->running_process->saved_cursor_pos;
    }
    return "";
}
void Console::resize(Layout layout) {
    if(running_process) {
        running_process->resize_on_next_output(this->layout);
    }
    this->layout = layout;
    this->pseudo_console->resize(layout.width, layout.height);
}
auto Console::get_layout() -> Layout& {
    return this->layout;
}
auto Console::wait_for_process_to_stop(int timeout) -> Alias::WAIT_RESULT {
    if(running_process) {
        return running_process->wait_for_stop(timeout);
    }
    return Alias::WAIT_RESULT::R_ERROR;
}