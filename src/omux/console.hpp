#pragma once
#include "action_factory.hpp"
#include "apis/alias.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <ostream>

namespace omux {
   /// constexpr auto PWSH_CONSOLE_PATH = L"F:\\dev\\projects\\PowerShell\\src\\powershell-win-core\\bin\\Debug\\net5.0\\pwsh.exe";
    constexpr auto PWSH_CONSOLE_PATH = L"F:\\dev\\bin\\pswh\\pwsh.exe";
    using Layout = struct Layout {
        int x;
        int y;
        int width;
        int height;
    };

    class OmuxError : public std::logic_error {
        public:
        OmuxError() : std::logic_error("Something went wrong in omux") {}
        OmuxError(std::string error) : std::logic_error(error){}
    };

    class Console;
    class Process;
    class PrimaryConsole;
    
    class Console {
        friend class Process;
        friend class PrimaryConsole;

        public:
        using Sptr = std::shared_ptr<Console>;
        Console(std::shared_ptr<PrimaryConsole>, Layout, Console*);
        Console(std::shared_ptr<PrimaryConsole>, Layout);
        ~Console();
        auto output() -> std::string;
        auto output_at(size_t) -> std::string_view;
        void process_attached(Process*);
        void process_dettached(Process*);
        auto is_running() -> bool;
        auto wait_for_process_to_stop(int) -> Alias::WAIT_RESULT;
        std::shared_ptr<PrimaryConsole> get_primary_console();
        auto get_scroll_buffer() -> std::vector<std::string>*;
        auto get_saved_cursor() -> std::string;
        void resize(Layout);
        auto get_layout() -> Layout&;

        private:
        Layout layout{0, 0, 0, 0};
        Process* running_process = nullptr;
        Alias::PseudoConsole::ptr pseudo_console;
        const std::shared_ptr<PrimaryConsole> primary_console;
        std::vector<std::string> scroll_buffer{std::string{}};
        bool first_process_added = false;
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
        auto wait_for_idle(int) -> Alias::WAIT_RESULT;
        auto wait_for_stop(int) -> Alias::WAIT_RESULT;
        auto process_running() -> bool;
        void process_output();
        void process_string_for_output(std::string_view);
        void output_line(std::string_view, std::string_view = "");
        void add_to_scrollbuffer(std::string_view);
        auto replace_bad_movement_command(std::string) -> std::string;
        auto track_cursor_for_sequence(std::string_view sequence) -> std::pair<int, int>;
        auto handle_csi_sequence(std::string_view::iterator& start, std::string_view::iterator& end) -> std::string_view::iterator;
        void set_line_in_screen(unsigned int line_in_screen);
        void output_line_from_scroll_buffer(std::string& output, std::ostream& line);
        void process_resize(std::string_view output);
        void resize_on_next_output(Layout);
        auto delete_n_renderable_characters_from_string(std::string& line, int n) -> std::string;

        private:
        Alias::Process::ptr process;
        std::thread output_thread;
        unsigned int line_in_screen = 1; // rows
        unsigned int characters_from_start = 1; // columns
        std::string saved_cursor_pos{"\x1b[1;1H"};
        std::atomic<bool> resize_on_next_output_flag = false;
        std::fstream command_log;
    };
    enum SPLIT_DIRECTION { VERT, HORI };
    
    class PrimaryConsole {
        Alias::MainConsole primary_console;
        omux::Console* active_console = nullptr;
        /**
         * As the active_console is going to called from a lambda thread
         * and being set from different places, we need to ensure we lock guard
         * it.
         */
        std::mutex active_console_lock;
        
        std::mutex stdout_mutex;
        std::thread stdin_read_thread;
        std::vector<Console*> attached_consoles;
        std::shared_ptr<omux::ActionFactory> action_factory;
        bool first_console_added = false;

        public:
        using Sptr = std::shared_ptr<PrimaryConsole>;
        PrimaryConsole();
        PrimaryConsole(std::shared_ptr<omux::ActionFactory>);
        virtual ~PrimaryConsole();
        void set_active(Console*);
        virtual void write_to_stdout(std::string_view);
        virtual void write_to_stdout(std::stringstream&);
        virtual auto write_character_to_stdout(const char) -> bool;
        void write_input(std::string_view);
        auto process_input(std::string_view) -> std::string;
        // TODO This should return an object which is the only way to
        // write to stdout
        void lock_stdout();
        void unlock_stdout();
        void wait_for_attached_consoles();
        void add_console(Console*);
        void remove_console(Console*);
        auto should_stop() -> bool;
        void reset_stdio();
        auto get_stdout_lock() -> std::mutex*;
        auto split_active_console(SPLIT_DIRECTION) -> Console::Sptr;
        auto get_terminal_size() -> Layout;
        auto get_active_console() -> Console* {
            return active_console;
        };
    };
    
} // namespace omux