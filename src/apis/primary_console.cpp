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
    auto PrimaryConsole::number_of_input_events() -> size_t{
        DWORD number_of_events = 0;
        if(PeekConsoleInput(
            std_in,
            nullptr,
            0,
            &number_of_events
        ) == 0){
            check_and_throw_error("Couldn't peek at read_pipe");
        }
        return number_of_events;
    }
    auto PrimaryConsole::write_to_stdout(std::string_view output) -> size_t {
        DWORD bytes_written = 0;
        if(!static_cast<bool>(WriteFile(this->std_out, output.data(), output.size()*sizeof(char), &bytes_written, nullptr))){
            check_and_throw_error("Couldn't write to stdout");
        }
        return bytes_written;
    }
    auto PrimaryConsole::read_input_from_console() -> std::future<std::string>{
        return std::async(std::launch::async, [&](){ 
            std::string input(16, '\0');
            DWORD bytes_read = 0;
            if(!static_cast<bool>(ReadFile(this->std_in, input.data(), input.size()*sizeof(char), &bytes_read, nullptr))){
                check_and_throw_error("Couldn't read from stdin");
            }

            return input.substr(0, bytes_read/sizeof(char));
        }); 
    }
}