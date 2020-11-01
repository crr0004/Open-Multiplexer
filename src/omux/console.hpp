#pragma once
#include <string_view>
#include <memory>
#include "apis/alias.hpp"

namespace omux{
    typedef struct Layout{
        int x;
        int y;
        int width;
        int height;
    } Layout;

    class Console;
    class Process;
    class Console{
        friend class Process;
        public:
            using Sptr = std::shared_ptr<Console>;
            Console(Layout, Console*);
            Console(Layout);
            std::shared_ptr<std::string> output();
        private:
            Alias::PseudoConsole::ptr pseudo_console;
    };

    class Process{
        friend class Console;
        public:
            const Console::Sptr host;
            const std::wstring path;
            const std::wstring args;
            Process(Console::Sptr, std::wstring, std::wstring);
            void wait_for_stop(unsigned long);
        private:
            Alias::Process::ptr process;
    } ;
}