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
    class PrimaryConsole;
    class Console{
        friend class Process;
        friend class PrimaryConsole;
        public:
            using Sptr = std::shared_ptr<Console>;
            const Layout layout;
            Console(Layout, Console*);
            Console(Layout);
            std::string output();
            std::string output_at(size_t);
            void process_attached(Process*);
            bool is_running();
        private:
            Process* running_process = nullptr;
            Alias::PseudoConsole::ptr pseudo_console;
            PrimaryConsole *primary_console;
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
            bool process_running();
        private:
            Alias::Process::ptr process;
            std::thread output_thread;
    };
    class PrimaryConsole{
        Alias::PrimaryConsole primary_console;
        Console::Sptr active_console;
        bool running = false;
        /**
        * As the active_console is going to called from a lambda thread
        * and being set from different places, we need to ensure we lock guard it.
        */
        std::mutex active_console_lock;
        std::mutex stdin_mutex;
        std::mutex stdout_mutex;
        std::thread stdin_read_thread;
        Alias::PrimaryConsole console;
        public:
            PrimaryConsole(Console::Sptr);
            ~PrimaryConsole();
            void set_active(Console::Sptr);
            void write_to_stdout(std::string_view);
            // TODO This should return an object which is the only way to
            // write to stdout
            void lock_stdout();
            void unlock_stdout();
            void join_read_thread() {
                if (stdin_read_thread.joinable())
                    stdin_read_thread.join();
            }
    };
}