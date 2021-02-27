#include "apis/alias.hpp"

namespace Alias {
    MainConsole::MainConsole() {
        this->std_in = GetStdHandle(STD_INPUT_HANDLE);        
        this->std_out = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    MainConsole::~MainConsole() {
        try {
            cancel_io();
        } catch(Alias::IO_Operation_Aborted& e) {
        } catch(Alias::Not_Found& e) {
        }
        
    }
    void MainConsole::cancel_io() {
        //SetLastError(0);
        DWORD error = 0;
        if(CancelIoEx(this->std_in, nullptr) == 0) {
            error = GetLastError();
        }
        if(CancelIoEx(this->std_out, nullptr) == 0) {
            error = 0;
            error = GetLastError();
        }
        this->std_in = nullptr;
        this->std_out = nullptr;
    }
    auto MainConsole::number_of_input_events() -> size_t {
        DWORD number_of_events = 0;
        if(GetNumberOfConsoleInputEvents(std_in, &number_of_events) == 0) {
            check_and_throw_error("Couldn't peek at read_pipe");
        }
        
        return number_of_events;
    }
    auto MainConsole::write_to_stdout(std::string_view output) -> size_t {
        DWORD bytes_written = 0;
        if(!static_cast<bool>(WriteFile(this->std_out, output.data(), output.size() * sizeof(char), &bytes_written, nullptr))) {
            check_and_throw_error("Couldn't write to stdout");
        }
        return bytes_written;
    }
    auto MainConsole::write_to_stdout(std::stringstream& output) -> size_t {
        DWORD bytes_written = 0;
        if(!static_cast<bool>(WriteFile(this->std_out, output.rdbuf(), output.tellp() * sizeof(char), &bytes_written, nullptr))) {
            check_and_throw_error("Couldn't write to stdout");
        }
        return bytes_written;
    }
    auto MainConsole::write_character_to_stdout(char output) -> bool {
        return static_cast<bool>(WriteFile(this->std_out, &output, 1, nullptr, nullptr));
    }
    auto MainConsole::read_input_from_console() -> std::future<std::string> {
        return std::async(std::launch::async, [&, this]() {
            std::string input(16, '\0');
            DWORD bytes_read = 0;
            if(std_in != nullptr && ReadFile(std_in, input.data(), input.size() * sizeof(char), &bytes_read, nullptr) == 0) {
                check_and_throw_error("Couldn't read from stdin");
            }
            

            return input.substr(0, bytes_read / sizeof(char));
        });
    }
    void MainConsole::reset_stdio() {
        this->std_out = GetStdHandle(STD_OUTPUT_HANDLE);
        this->std_in = GetStdHandle(STD_INPUT_HANDLE);
    }
} // namespace Alias