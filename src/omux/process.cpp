#include "omux/console.hpp"
#include "apis/alias.hpp"

namespace omux{
    Process::Process(Console::Sptr host, std::wstring path, std::wstring args)
        : host(host), path(path), args(args) {
            this->process = Alias::NewProcess(
                host->pseudo_console.get(), 
                path + args
                );
    }
    void Process::wait_for_stop(unsigned long timeout){
        this->process->wait_for_stop(timeout);

    }
}