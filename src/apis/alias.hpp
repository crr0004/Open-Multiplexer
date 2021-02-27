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
    class IO_Operation_Aborted : public WindowsError {
        public:
        IO_Operation_Aborted() : WindowsError("IO Operation aborted") {
        }
    };
    class Not_Found : public WindowsError {
        public:
        Not_Found() : WindowsError("Resource not found. Can probably ignore this.") {
        }
    };
    class Process;
    class PseudoConsole;
    class MainConsole;

    void check_and_throw_error(HRESULT error) noexcept(false);
    void check_and_throw_error(std::string error_message) noexcept(false);
    void check_and_throw_error() noexcept(false);
    auto CreatePseudoConsole(int, int, short, short) noexcept(false) -> std::unique_ptr<PseudoConsole>;
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
    auto Get_Terminal_Size() -> std::pair<short, short>;

    class MainConsole {
        private:
        std::atomic<HANDLE> std_in;
        std::atomic<HANDLE> std_out;

        public: 
        MainConsole();
        ~MainConsole();
        void cancel_io();
        auto read_input_from_console() -> std::future<std::string>;
        auto number_of_input_events() -> size_t;
        auto write_to_stdout(std::string_view) -> size_t;
        auto write_to_stdout(std::stringstream&) -> size_t;
        auto write_character_to_stdout(char output) -> bool;
        void reset_stdio();
    };
    class PseudoConsole {
        private:
        std::atomic<HANDLE> pipe_in;
        std::atomic<HANDLE> pipe_out;
        public:
        using ptr = std::unique_ptr<PseudoConsole>;
        using Sptr = std::shared_ptr<PseudoConsole>;
        using BufferIterator = std::vector<std::string>::iterator;
        const int x;
        const int y;
        
        const HPCON pseudo_console_handle;
        std::string last_read_in;

        // TODO Rather than making the following calls virtual for testing,
        // refactor to using templates for the owning objects

        PseudoConsole(int x, int y, HPCON pseudoConsoleHandle, HANDLE pipeIn, HANDLE pipeOut)
        : pipe_in(pipeIn), pipe_out(pipeOut), x(x), y(y), pseudo_console_handle(pseudoConsoleHandle) {
            // this->write_input("\x1b[5G\r\n");
        }

        ~PseudoConsole() {

            ClosePseudoConsole(pseudo_console_handle);
            if(pipe_in != 0) {
                CloseHandle(pipe_in);
            }
            if(pipe_out != 0) {
                CloseHandle(pipe_out);
            }            
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
        void close_pipes();
        void resize(short, short);
    };
    enum WAIT_RESULT { SUCCESS, TIMEOUT, R_ERROR };
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
        [[nodiscard]] bool stopped() const;
        auto wait_for_idle(int timeout) const -> WAIT_RESULT {
            unsigned long actual_timeout = 0;
            if(timeout < 0) {
                actual_timeout = INFINITE;
            } else {
                actual_timeout = static_cast<unsigned long>(timeout);
            }
            auto result = WaitForInputIdle(process_info.hProcess, timeout);
            switch(result) {
                case 0:
                    return WAIT_RESULT::SUCCESS;
                case WAIT_TIMEOUT:
                    return WAIT_RESULT::TIMEOUT;
                case WAIT_FAILED:
                default:
                    check_and_throw_error("Couldn't wait for process to idle: " + std::to_string(static_cast<unsigned int>(process_info.dwProcessId)));
                    return WAIT_RESULT::R_ERROR;
            }
        }
        auto wait_for_stop(int timeout) const -> WAIT_RESULT {
            unsigned long actual_timeout = 0;
            if(timeout < 0) {
                actual_timeout = INFINITE;
            }
            else {
                actual_timeout = static_cast<unsigned long>(timeout);
            }
            auto result = WaitForSingleObject(process_info.hProcess, timeout);
            switch (result) {
                case WAIT_OBJECT_0:
                    return WAIT_RESULT::SUCCESS;
                case WAIT_TIMEOUT:
                    return WAIT_RESULT::TIMEOUT;
                case WAIT_ABANDONED:
                case WAIT_FAILED:
                default:
                    return WAIT_RESULT::R_ERROR;
            }
        }
        ~Process() {
            this->kill(INFINITE);
            CloseHandle(process_info.hThread);
            CloseHandle(process_info.hProcess);
            SetLastError(0); // Ignore any errors generated by closing
        }
    };
    
} // namespace Alias
