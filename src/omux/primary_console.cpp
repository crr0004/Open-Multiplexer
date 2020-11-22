#include "omux/console.hpp"
#include "apis/alias.hpp"
#include <mutex>
#include <thread>
#include <chrono>
#include <future>

namespace omux{
    
    PrimaryConsole::PrimaryConsole(Console::Sptr active_console){
        this->active_console = active_console;
        this->running = true;
        active_console->primary_console = this;
        stdin_read_thread = std::thread([&](){
            std::scoped_lock stdin_lock(stdin_mutex);
            auto input_read = console.read_input_from_console();
            do{
                    auto result = input_read.wait_for(std::chrono::milliseconds(16));
                    if(result == std::future_status::ready){
                        std::scoped_lock lock(active_console_lock);
                        this->active_console->pseudo_console->write_input(input_read.get());
                        input_read = console.read_input_from_console();
                    }

            }while(this->running);
        });

    }
    void PrimaryConsole::lock_stdout(){
        this->active_console_lock.lock();
    }
    void PrimaryConsole::unlock_stdout(){
        this->active_console_lock.unlock();
    }
    void PrimaryConsole::write_to_stdout(std::string_view output){
        this->primary_console.write_to_stdout(output);

    }
    PrimaryConsole::~PrimaryConsole(){
        primary_console.cancel_io();
        this->running = false;
        if(stdin_read_thread.joinable()){
            
            stdin_read_thread.join();
        }
    }
    void PrimaryConsole::set_active(Console::Sptr console){
        std::scoped_lock lock(active_console_lock);
        this->active_console = console;
    }
}