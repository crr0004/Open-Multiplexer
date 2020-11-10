#pragma once
#include <memory>
#include "apis/alias.hpp"
#include <thread>

namespace omux{
    typedef struct Layout{
        int x;
        int y;
        int width;
        int height;
    } Layout;

    bool SetupConsoleHost() noexcept(false);
    void WriteToStdOut(std::string message);

    class Console;
    class Process;
    class Console{
        friend class Process;
        public:
            using Sptr = std::shared_ptr<Console>;
            const Layout layout;
            Console(Layout, Console*);
            Console(Layout);
            std::string output();
            std::string output_at(size_t);
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
            ~Process();
            void wait_for_stop(unsigned long);
        private:
            Alias::Process::ptr process;
            std::thread output_thread;
    } ;
}