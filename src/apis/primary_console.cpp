#include "apis/alias.hpp"

namespace Alias{
    PrimaryConsole::PrimaryConsole(){
        this->std_in = GetStdHandle(STD_INPUT_HANDLE);
        this->std_out = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    PrimaryConsole::~PrimaryConsole() {
        cancel_io();
    }
    void PrimaryConsole::cancel_io() {
        CancelIoEx(std_in, nullptr);
        CancelIoEx(std_out , nullptr);
    }
    size_t PrimaryConsole::number_of_input_events(){
        DWORD number_of_events = 0;
        if(PeekConsoleInput(
            std_in,
            nullptr,
            0,
            &number_of_events
        ) == false){
            check_and_throw_error("Couldn't peek at read_pipe");
        }
        return number_of_events;
    }
    size_t PrimaryConsole::write_to_stdout(std::string_view output) {
        DWORD bytes_written = 0;
        if(WriteFile(this->std_out, output.data(), output.size()*sizeof(char), &bytes_written, nullptr) == false){
            check_and_throw_error("Couldn't write to stdout");
        }
        return bytes_written;
    }
    std::future<std::string> PrimaryConsole::read_input_from_console(){
        return std::async(std::launch::async, [&](){ 
            std::string input(16, '\0');
            DWORD bytes_read = 0;
            if(ReadFile(this->std_in, input.data(), input.size()*sizeof(char), &bytes_read, nullptr) == false){
                check_and_throw_error("Couldn't read from stdin");
            }

            return input.substr(0, bytes_read/sizeof(char));
        }); 
    }
}