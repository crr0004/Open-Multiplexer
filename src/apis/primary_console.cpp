#include "apis/alias.hpp"

namespace Alias{
    PrimaryConsole::PrimaryConsole(){
        this->std_in = GetStdHandle(STD_INPUT_HANDLE);
    }
    size_t PrimaryConsole::bytes_in_input_pipe(){
        DWORD bytes_in_pipe = 0;
        if(PeekNamedPipe(
            std_in,
            nullptr, // don't want to actually copy the data
            0,
            nullptr,
            &bytes_in_pipe,
            nullptr	
        ) == false){
            check_and_throw_error("Couldn't peek at read_pipe");
        }
        return bytes_in_pipe;
    }
    std::wstring PrimaryConsole::read_input_from_console(){
        std::wstring input(512, '\0');
        DWORD bytes_read = 0;
        if(ReadFile(std_in, input.data(), input.size()*sizeof(wchar_t), &bytes_read, nullptr) == false){
            check_and_throw_error("Couldn't read from stdin");
        }

        return input.substr(0, bytes_read/sizeof(wchar_t));
    }
}