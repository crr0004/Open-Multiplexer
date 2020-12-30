#pragma once
#include "apis/alias.hpp"
#include <memory>
#include <thread>

namespace omux {
    using Layout = struct Layout {
        int x;
        int y;
        int width;
        int height;
    };

    auto SetupConsoleHost() noexcept(false) -> bool;
    auto ReverseSetupConsoleHost() noexcept(false) -> bool;
    void WriteToStdOut(std::string message);

    class Console;
    class Process;
    class PrimaryConsole;
    class Console {
        friend class Process;
        friend class PrimaryConsole;

          public:
        using Sptr = std::shared_ptr<Console>;
        const Layout layout;
        Console(std::shared_ptr<PrimaryConsole>, Layout, Console*);
        Console(std::shared_ptr<PrimaryConsole>, Layout);
        ~Console();
        auto output() -> std::string;
        auto output_at(size_t) -> std::string;
        void process_attached(Process*);
        void process_dettached(Process*);
        auto is_running() -> bool;
        std::shared_ptr<PrimaryConsole> get_primary_console();

          private:
        Process* running_process = nullptr;
        Alias::PseudoConsole::ptr pseudo_console;
        const std::shared_ptr<PrimaryConsole> primary_console;
    };

    class Process {
        friend class Console;

          public:
        using Sptr = std::shared_ptr<Process>;
        const Console::Sptr host;
        const std::wstring path;
        const std::wstring args;
        Process(Console::Sptr, std::wstring, std::wstring);
        Process(Console::Sptr);
        ~Process();
        void wait_for_stop(unsigned long);
        auto process_running() -> bool;
        void process_output();
        auto get_starting_point_for_write(std::vector<std::string>*,
                                          std::vector<std::string>::iterator,
                                          unsigned int) -> std::vector<std::string>::iterator;

          private:
        Alias::Process::ptr process;
        std::thread output_thread;
    };
    class PrimaryConsole {
        Alias::PrimaryConsole primary_console;
        Console* active_console = nullptr;
        bool running = false;
        /**
         * As the active_console is going to called from a lambda thread
         * and being set from different places, we need to ensure we lock guard
         * it.
         */
        std::mutex active_console_lock;
        std::mutex stdin_mutex;
        std::mutex stdout_mutex;
        std::thread stdin_read_thread;
        Alias::PrimaryConsole console;
        std::vector<Console*> attached_consoles;

          public:
        using Sptr = std::shared_ptr<PrimaryConsole>;
        PrimaryConsole();
        ~PrimaryConsole();
        void set_active(Console::Sptr);
        void write_to_stdout(std::string_view);
        // TODO This should return an object which is the only way to
        // write to stdout
        void lock_stdout();
        void unlock_stdout();
        void join_read_thread();
        void add_console(Console*);
        void remove_console(Console*);
        auto should_stop() -> bool;
    };
} // namespace omux