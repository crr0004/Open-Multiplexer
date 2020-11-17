#include "omux/console.hpp"
#include "apis/alias.hpp"
#include <chrono>
#include <thread>
#include <memory>
#include <algorithm>
#include <mutex>

namespace omux{
    std::mutex std_out_mutex;
    Process::Process(Console::Sptr host, std::wstring path, std::wstring args)
        : host(host), path(path), args(args) {
            this->process = Alias::NewProcess(
                host->pseudo_console.get(), 
                path + args
                );
                this->output_thread = std::thread(
                    [&](){
                        auto pseudo_console = host->pseudo_console.get();
                        std::wstring cursor_pos = L"\x1b[" + std::to_wstring(host->layout.y) + L";"+ std::to_wstring(host->layout.x) +L"H";
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
                               auto start = pseudo_console->read_output();
                               auto end = pseudo_console->get_output_buffer()->end();
                               {
                                //    std::lock_guard std_out_lock(std_out_mutex);
                                   std_out_mutex.lock();
                                   // TODO need to find a way to save and restore the cursor
                                   // for when the threads switch between writing to stdout
                                   if(!cursor_pos.empty()){
                                        std::wstring cursor_save_flag{L"aaa"};
                                        // pseudo_console->write_to_stdout(cursor_save_flag);
                                        pseudo_console->write_to_stdout(cursor_pos);
                                   }
                                   while(start != end){
                                        auto output = *start;
                                        std::wstring move_to_column{L"\x1b[" + std::to_wstring(host->layout.x) + L"G"};
                                        pseudo_console->write_to_stdout(move_to_column);
                                        pseudo_console->write_to_stdout(output);
                                        start++;
                                        if(start == end){
                                            std::wstring cursor_save_flag{L"bbb"};
                                            // pseudo_console->write_to_stdout(cursor_save_flag);
                                            cursor_pos = pseudo_console->get_cursor_position_as_movement();
                                            // pseudo_console->write_to_stdout(cursor_pos);
                                        }
                                    }
                                   std_out_mutex.unlock();
                               }
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