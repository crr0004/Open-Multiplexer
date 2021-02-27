#include "apis/alias.hpp"
#include "omux/console.hpp"

#include <chrono>
#include <fstream>
#include <future>
#include <mutex>
#include <thread>

using namespace omux;



PrimaryConsole::PrimaryConsole() : PrimaryConsole(std::make_shared<ActionFactory>()) {
}

PrimaryConsole::PrimaryConsole(std::shared_ptr<ActionFactory> action_factory) : action_factory(action_factory) {
    stdin_read_thread = std::thread([&]() {
        std::future<std::string> input_read;
        try {
        
            input_read = this->primary_console.read_input_from_console();
            while(!this->should_stop()) {

                auto result = input_read.wait_for(std::chrono::milliseconds(16));
                if(result == std::future_status::ready) {
                    process_input(input_read.get());
                    input_read = primary_console.read_input_from_console();
                }

            }
        
            primary_console.cancel_io();
        } catch(Alias::IO_Operation_Aborted e) {
        } catch(Alias::Not_Found e) {
        }
    });
}

auto PrimaryConsole::process_input(std::string_view input) -> std::string {
    std::string processed_input{input};
    if(action_factory->process_to_action(processed_input) == Actions::none) {
        std::scoped_lock lock(active_console_lock);
        if(active_console != nullptr && active_console->is_running()) {
            this->active_console->pseudo_console->write_input(processed_input);
        }
    } else {
        action_factory->get_action_stack()->back()->act(this);
    }
    return processed_input;
}

void PrimaryConsole::write_input(std::string_view input) {
    if(active_console != nullptr) {
        std::scoped_lock lock(active_console_lock);
        this->active_console->pseudo_console->write_input(input);
    }
}
void PrimaryConsole::reset_stdio() {
    primary_console.reset_stdio();
}
[[nodiscard]] auto PrimaryConsole::should_stop() -> bool {
    bool all_processes_done = attached_consoles.empty();
    
    
    for(auto console : attached_consoles) {
        all_processes_done = all_processes_done || !console->is_running();
    }

    auto result = first_console_added && all_processes_done;
    return result;
}
[[nodiscard]] auto PrimaryConsole::get_stdout_lock() -> std::mutex* {
    return &this->stdout_mutex;
}
void PrimaryConsole::lock_stdout() {
    this->stdout_mutex.lock();
}
void PrimaryConsole::unlock_stdout() {
    if(active_console != nullptr) {
        write_to_stdout(active_console->get_saved_cursor());
    }        
    this->stdout_mutex.unlock();
}
void PrimaryConsole::write_to_stdout(std::string_view output) {
    //std::scoped_lock lock{stdout_mutex};

    this->primary_console.write_to_stdout(output);
}
void PrimaryConsole::write_to_stdout(std::stringstream& output) {

    //std::scoped_lock lock{stdout_mutex};
    this->primary_console.write_to_stdout(output);
}
auto PrimaryConsole::write_character_to_stdout(const char output) -> bool {
    // std::scoped_lock lock{stdout_mutex};

    return this->primary_console.write_character_to_stdout(output);
}
void PrimaryConsole::wait_for_attached_consoles() {
    for(auto* console : attached_consoles) {
        console->wait_for_process_to_stop(-1);
    }
}
void PrimaryConsole::add_console(Console* console) {
    first_console_added = true;
    this->attached_consoles.push_back(console);
}
void PrimaryConsole::remove_console(Console* console) {
    std::scoped_lock lock(active_console_lock);
    auto console_to_remove = std::find(this->attached_consoles.begin(), this->attached_consoles.end(), console);
    this->attached_consoles.erase(console_to_remove);
    if(console == active_console && !attached_consoles.empty()) {
        
        this->active_console = attached_consoles.front();
    }
}
 PrimaryConsole::~PrimaryConsole() {
    //stdin_read_thread.detach();
    if(this->stdin_read_thread.joinable()) {
         this->stdin_read_thread.join();
    }
    
}
void PrimaryConsole::set_active(Console* new_active_console) {
    std::scoped_lock lock(active_console_lock);
    this->active_console = new_active_console;
}


[[nodiscard]] auto PrimaryConsole::split_active_console(SPLIT_DIRECTION direction) -> Console::Sptr {
    std::scoped_lock lock(active_console_lock);
    if(active_console == nullptr) {
        throw OmuxError("Trying to split the active console when it hasn't been set yet");
    }
    Layout split_layout{active_console->get_layout()};
    if(direction == SPLIT_DIRECTION::VERT) {
        split_layout.width /= 2;
        split_layout.width--;
        active_console->resize(Layout{split_layout});
        split_layout.x = split_layout.width+2;
       // split_layout.width;
            
    } else {
        split_layout.height /= 2;
        split_layout.height--;
        active_console->resize(Layout{split_layout});
        split_layout.y = split_layout.height+2;
        
    }
    return std::make_shared<Console>(active_console->get_primary_console(), Layout{split_layout});
}

[[nodiscard]] auto PrimaryConsole::get_terminal_size() -> Layout {
    Layout layout{0, 0, 0, 0};
    auto terminal_size = Alias::Get_Terminal_Size();
    layout.width = terminal_size.first;
    layout.height = terminal_size.second;
    return layout;
}