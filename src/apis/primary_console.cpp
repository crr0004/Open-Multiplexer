#include "apis/alias.hpp"

namespace Alias{
    PrimaryConsole::PrimaryConsole(){
        this->std_in = GetStdHandle(STD_INPUT_HANDLE);
    }
    std::string PrimaryConsole::read_input_from_console(){
        std::string input(512, '\0');
        DWORD bytes_read = 0;
        if(ReadFile(std_in, input.data(), input.size()*sizeof(char), &bytes_read, nullptr) == false){
            check_and_throw_error("Couldn't read from stdin");
        }

        return input.substr(0, bytes_read/sizeof(char));
    }
}