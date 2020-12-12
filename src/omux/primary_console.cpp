#include "omux/console.hpp"
#include "apis/alias.hpp"
#include <mutex>
#include <thread>
#include <chrono>
#include <future>
#include <fstream>

namespace omux{
    
    std::fstream log_file;
    PrimaryConsole::PrimaryConsole(){
        log_file.open("pty.log");
        this->running = true;
        stdin_read_thread = std::thread([&]() {
            std::scoped_lock stdin_lock(stdin_mutex);
            auto input_read = this->console.read_input_from_console();
            do {
                
                auto result = input_read.wait_for(std::chrono::milliseconds(16));
                if (active_console != nullptr && result == std::future_status::ready) {
                    std::scoped_lock lock(active_console_lock);
                    this->active_console->pseudo_console->write_input(input_read.get());
                    input_read = this->console.read_input_from_console();
                }

            } while (!this->should_stop());
            primary_console.cancel_io();
        });
    }
    auto PrimaryConsole::should_stop() -> bool {
        return attached_consoles.empty();
    }
    void PrimaryConsole::lock_stdout(){
        this->active_console_lock.lock();
    }
    void PrimaryConsole::unlock_stdout(){
        this->active_console_lock.unlock();
    }
    void PrimaryConsole::write_to_stdout(std::string_view output){
        this->primary_console.write_to_stdout(output);
        log_file.write(output.data(), output.size());
        log_file.flush();

    }
    void PrimaryConsole::join_read_thread(){
        if (stdin_read_thread.joinable()) {
            stdin_read_thread.join();
        }
        for(auto* console : attached_consoles){
            console->running_process->wait_for_stop(-1);
        }
    }
    void PrimaryConsole::add_console(Console* console){
        this->attached_consoles.push_back(console);
    }
    void PrimaryConsole::remove_console(Console* console) {
        auto console_to_remove = std::find(this->attached_consoles.begin(), this->attached_consoles.end(), console);
        this->attached_consoles.erase(console_to_remove);
        if (console == active_console && !attached_consoles.empty()) {
            std::scoped_lock lock(active_console_lock);
            active_console = attached_consoles.front();
        }
    }
    PrimaryConsole::~PrimaryConsole(){
        primary_console.cancel_io();
        this->running = false;
        join_read_thread(); 
    }
    void PrimaryConsole::set_active(Console::Sptr new_active_console){
        std::scoped_lock lock(active_console_lock);
        this->active_console = new_active_console.get();
        
    }
}