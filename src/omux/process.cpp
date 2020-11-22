#include "omux/console.hpp"
#include "apis/alias.hpp"
#include <chrono>
#include <thread>
#include <memory>
#include <algorithm>
#include <mutex>

namespace omux{
    Process::Process(Console::Sptr host, std::wstring path, std::wstring args)
        : host(host), path(path), args(args) {
            this->process = Alias::NewProcess(
                host->pseudo_console.get(), 
                path + args
                );
                this->output_thread = std::thread(
                    [&](){
                        auto pseudo_console = host->pseudo_console.get();
                        auto primary_console = host->primary_console;
                        std::string cursor_pos = "\x1b[" + std::to_string(host->layout.y) + ";"+ std::to_string(host->layout.x) +"H";
                        while(
                            !this->process->stopped() || 
                            pseudo_console->bytes_in_read_pipe() > 0
                            ){
                            /* 
                            We need to guard against reading an empty pipe
                            because windows doesn't support read timeout for pipes
                            The ReadFile function that backs the API blocks when no data is avaliable
                            and pipes can't have a timeout assiocated with them for REASONS (SetCommTimeout just throws error)
                            */
                            if(pseudo_console->bytes_in_read_pipe() > 0){
                                // TODO Refactor to use futures like in omux/primary_console.cpp
                                auto start = pseudo_console->read_output();
                                auto end = pseudo_console->get_output_buffer()->end();
                                // TODO this is terrible design. Too easy to forget to lock the object
                                primary_console->lock_stdout();
                                // TODO Refactor so the lock for stdout is grabed and release for the whole loop
                                primary_console->write_to_stdout(cursor_pos);
                                while(start != end){
                                    auto output = *start;
                                    std::string move_to_column{"\x1b[" + std::to_string(host->layout.x) + "G"};
                                    primary_console->write_to_stdout(move_to_column);
                                    primary_console->write_to_stdout(output);
                                    start++;
                                    if(start == end){
                                        cursor_pos = pseudo_console->get_cursor_position_as_movement();
                                    }
                                }
                                primary_console->unlock_stdout();
                            }else{
                               std::this_thread::sleep_for(
                                   std::chrono::milliseconds(Alias::OUTPUT_LOOP_SLEEP_TIME_MS)
                               );
                            }
                        }
                    }
                );
    }
    Process::~Process(){
        if(output_thread.joinable()){
            output_thread.join();
        }
    }
    void Process::wait_for_stop(unsigned long timeout){
        this->process->wait_for_stop(timeout);
    }
}