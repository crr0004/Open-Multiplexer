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
        auto output_at(size_t) -> std::string_view;
        void process_attached(Process*);
        void process_dettached(Process*);
        auto is_running() -> bool;
        std::shared_ptr<PrimaryConsole> get_primary_console();
        auto get_scroll_buffer() -> std::vector<std::string>*;

        private:
        Process* running_process = nullptr;
        Alias::PseudoConsole::ptr pseudo_console;
        const std::shared_ptr<PrimaryConsole> primary_console;
        std::vector<std::string> scroll_buffer{""};
    };

    class Process {
        friend class Console;

        public:
        using Sptr = std::shared_ptr<Process>;
        const Console::Sptr host;
        const std::wstring path;
        const std::wstring args;
        std::string continuing_output_line;
        Process(Console::Sptr, std::wstring, std::wstring);
        Process(Console::Sptr);
        ~Process();
        void wait_for_stop(unsigned long);
        auto process_running() -> bool;
        void process_output();
        void process_string_for_output(std::string_view);
        void output_line(std::string_view, std::string_view = "");
        void add_to_scrollbuffer(std::string_view);
        auto replace_bad_movement_command(std::string) -> std::string;
        auto track_cursor_for_sequence(std::string_view sequence) -> std::pair<int, int>;
        auto handle_csi_sequence(std::string_view::iterator& start, std::string_view::iterator& end) -> std::string_view::iterator;
        void set_line_in_screen(unsigned int line_in_screen);


        private:
        Alias::Process::ptr process;
        std::thread output_thread;
        unsigned int line_in_screen = 1; // rows
        unsigned int characters_from_start = 1; // columns
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
        auto write_character_to_stdout(char output) -> bool;
        // TODO This should return an object which is the only way to
        // write to stdout
        void lock_stdout();
        void unlock_stdout();
        void join_read_thread();
        void add_console(Console*);
        void remove_console(Console*);
        auto should_stop() -> bool;
        void reset_stdout();
    };
} // namespace omux