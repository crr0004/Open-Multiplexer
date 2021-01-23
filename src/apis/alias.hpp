#pragma once
#include <Windows.h>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <process.h>
#include <sdkddkver.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Alias {
    constexpr size_t COMM_TIMEOUT = 500;
    constexpr size_t READ_BUFFER_SIZE = 16384;
    constexpr size_t OUTPUT_LOOP_SLEEP_TIME_MS = 16; // millisecond
    class WindowsError : public std::logic_error {
        public:
        WindowsError(long error)
        : std::logic_error("Something went wrong in the windows API calls. "
                           "Error: " +
                           std::to_string(error)) {
            // TODO integrate this code to format windows error
            /*
                auto error = GetLastError();
                LPVOID lpMsgBuf;
                LPVOID lpDisplayBuf;

                FormatMessage(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER |
                    FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,
                    error,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR)&lpMsgBuf,
                    0, NULL);
                std::wstring wmessage((LPTSTR)lpMsgBuf);
                std::string message;
                message.assign(wmessage.begin(), wmessage.end());
            */
        }
        WindowsError(std::string error_message) : std::logic_error(error_message) {
        }
        const std::string message = this->what();
    };
    class Process;
    class PseudoConsole;
    class PrimaryConsole;

    class PrimaryConsole {
        private:
        HANDLE std_in;
        HANDLE std_out;

        public:
        PrimaryConsole();
        ~PrimaryConsole();
        void cancel_io();
        auto read_input_from_console() -> std::future<std::string>;
        auto number_of_input_events() -> size_t;
        auto write_to_stdout(std::string_view) -> size_t;
        auto write_to_stdout(std::stringstream&) -> size_t;
        auto write_character_to_stdout(char output) -> bool;
        void reset_stdout();
    };
    class PseudoConsole {
        public:
        using ptr = std::unique_ptr<PseudoConsole>;
        using Sptr = std::shared_ptr<PseudoConsole>;
        using BufferIterator = std::vector<std::string>::iterator;
        const int x;
        const int y;
        const HANDLE pipe_in;
        const HANDLE pipe_out;
        const HANDLE pty_pipe_out;
        const HPCON pseudo_console_handle;
        std::string last_read_in;

        // TODO Rather than making the following calls virtual for testing,
        // refactor to using templates for the owning objects

        PseudoConsole(int x, int y, HPCON pseudoConsoleHandle, HANDLE pipeIn, HANDLE pipeOut)
        : x(x), y(y), pipe_in(pipeIn), pipe_out(pipeOut), pty_pipe_out(0), pseudo_console_handle(pseudoConsoleHandle) {
            // this->write_input("\x1b[5G\r\n");
        }
        PseudoConsole(int x, int y, HPCON pseudoConsoleHandle, HANDLE pipeIn, HANDLE pipeOut, HANDLE pty_pipe_out)
        : x(x), y(y), pipe_in(pipeIn), pipe_out(pipeOut), pty_pipe_out(pty_pipe_out),
          pseudo_console_handle(pseudoConsoleHandle) {
            // this->write_input("\x1b[5G\r\n");
        }

        ~PseudoConsole() {

            ClosePseudoConsole(pseudo_console_handle);
            CloseHandle(pipe_in);
            CloseHandle(pipe_out);
        }
        auto read_output() -> std::future<std::string>;
        auto read_unbuffered_output() -> std::string;
        void write_input(std::string_view input) const;
        void write_to_pty_stdout(std::string_view input) const;
        [[nodiscard]] size_t bytes_in_read_pipe() const;
        [[nodiscard]] auto latest_output() const -> std::string {
            return last_read_in;
        }
        static auto get_cursor_position_as_vt(int, int) -> std::string;
        static auto get_cursor_position_as_movement() -> std::string;
        static auto get_cursor_position_as_pair() -> std::pair<unsigned int, unsigned int>;
        void process_attached(Process* process);
        void cancel_io_on_pipes();
    };
    class Process {

        public:
        using ptr = std::unique_ptr<Process>;
        using Sptr = std::shared_ptr<Process>;
        const STARTUPINFOEXW startup_info;
        const PROCESS_INFORMATION process_info;

        Process() : startup_info(STARTUPINFOEXW{}), process_info(PROCESS_INFORMATION{}) {
        }
        Process(STARTUPINFOEXW startup_info, PROCESS_INFORMATION process_info)
        : startup_info(startup_info), process_info(process_info) {
        }
        void kill(unsigned long timeout) const {
            TerminateProcess(process_info.hProcess, 0);
            WaitForSingleObject(process_info.hProcess, timeout);
        }
        // TODO Rather than making the following calls virtual for testing,
        // refactor to using templates for the owning objects
        [[nodiscard]] bool stopped() const;
        void wait_for_stop(unsigned long timeout) const {
            WaitForSingleObject(process_info.hThread, timeout);
        }
        ~Process() {
            this->kill(INFINITE);
            CloseHandle(process_info.hThread);
            CloseHandle(process_info.hProcess);
            SetLastError(0); // Ignore any errors generated by closing
        }
    };
    void check_and_throw_error(HRESULT error) noexcept(false);
    void check_and_throw_error(std::string error_message) noexcept(false);
    void check_and_throw_error() noexcept(false);
    auto CreatePseudoConsole(int, int, short, short) noexcept(false) -> PseudoConsole::ptr;
    auto CreateStartupInfoForConsole(PseudoConsole* console) noexcept(false) -> STARTUPINFOEXW;
    [[nodiscard]] auto NewProcess(PseudoConsole* console, std::wstring command_line) noexcept(false) -> Process*;
    auto GetCursorInfo(HANDLE console) -> CONSOLE_SCREEN_BUFFER_INFO;
    auto WriteToStdOut(std::string message) -> bool;
    auto SetupConsoleHost() -> bool;
    auto ReverseSetupConsoleHost() -> bool;
    auto Setup_Console_Stdout(HANDLE) -> std::string;
    auto Setup_Console_Stdin(HANDLE) -> std::string;
    void Apply_On_Split_String(std::string_view, char, std::function<void(std::string&)>);
    /**
     * Create a stdin and stdout pair based on fstreams.
     * This causes the stdin and stdout to be eaten basically.
     */
    auto Rebind_Std_In_Out() -> std::pair<std::fstream, std::fstream>;
    auto Get_StdIn_As_Stream() -> std::pair<std::ifstream, std::ofstream>;
    auto Get_StdOut_As_Stream() -> std::pair<std::ifstream, std::ofstream>;
    void Cancel_IO_On_StdOut();
    void Reset_StdHandles_To_Real();
} // namespace Alias
