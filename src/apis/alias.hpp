#pragma once
#include <SDKDDKVer.h>
#include <Windows.h>
#include <process.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <utility>
#include <string_view>

namespace Alias{
	constexpr size_t COMM_TIMEOUT = 500;
	constexpr size_t READ_BUFFER_SIZE = 1028;
	constexpr size_t OUTPUT_LOOP_SLEEP_TIME_MS = 16; // millisecond
	class WindowsError : public std::logic_error{
		public:
			WindowsError(long error) : std::logic_error(
				"Something went wrong in the windows API calls. Error: " +
				std::to_string(error)) {}
			WindowsError(std::string error_message) : std::logic_error(error_message){}
			const std::string message = this->what();
	};
	class Process;
    class PseudoConsole;
	class PrimaryConsole;

	class PrimaryConsole{
		private:
			HANDLE std_in;
		public:
			PrimaryConsole();
			std::wstring read_input_from_console();
			size_t bytes_in_input_pipe();
	};
    class PseudoConsole{
		private:
			std::vector<std::wstring> output_buffer{};
        public:
            using ptr = std::unique_ptr<PseudoConsole>;
            using Sptr = std::shared_ptr<PseudoConsole>;
			using BufferIterator = std::vector<std::wstring>::iterator;
			const int x;
			const int y;
            const HANDLE pipe_in;
            const HANDLE pipe_out;
            const HPCON pseudo_console_handle;
			std::wstring last_read_in;

			PseudoConsole() = delete;
			PseudoConsole(int x, int y, HPCON pseudoConsoleHandle, HANDLE pipeIn, HANDLE pipeOut)
			: x(x), y(y), pipe_in(pipeIn), pipe_out(pipeOut), pseudo_console_handle(pseudoConsoleHandle){}

            ~PseudoConsole(){

				ClosePseudoConsole(pseudo_console_handle);
				CloseHandle(pipe_in);
				CloseHandle(pipe_out);
            }
			BufferIterator read_output();
			void write_input(std::wstring_view);
			size_t bytes_in_read_pipe();
			auto get_output_buffer(){
				return &output_buffer;
			};
			std::wstring latest_output(){
				return last_read_in;
			}
			std::pair<BufferIterator, BufferIterator> read_output_as_pair(){
				return std::make_pair(this->read_output(), output_buffer.end());
			}
			std::wstring get_cursor_position_as_vt(int, int);
			std::wstring get_cursor_position_as_movement();
			size_t write_to_stdout(std::wstring&);
			size_t write(std::wstring&, HANDLE);
			void process_attached(Process *process);
    };
	class Process{
		public:
			using ptr = std::unique_ptr<Process>;
			const STARTUPINFOEXW startup_info;
			const PROCESS_INFORMATION process_info;

			Process(auto startup_info, auto process_info) : 
				startup_info(startup_info), process_info(process_info){}
			void kill(unsigned long timeout){
				TerminateProcess(process_info.hProcess, 0);
				WaitForSingleObject(process_info.hProcess, timeout);
			}
			bool stopped();
			void wait_for_stop(unsigned long timeout){
				WaitForSingleObject(process_info.hThread, timeout);
			}
			~Process(){
				this->kill(INFINITE);
				CloseHandle(process_info.hThread);
				CloseHandle(process_info.hProcess);
				SetLastError(0); // Ignore any errors generated by closing
			}
	};
	void check_and_throw_error(HRESULT error) noexcept(false);
	void check_and_throw_error(std::string error_message) noexcept(false);
	void check_and_throw_error() noexcept(false);
    PseudoConsole::ptr CreatePseudoConsole(int, int, int, int) noexcept(false);
	STARTUPINFOEXW CreateStartupInfoForConsole(PseudoConsole *console) noexcept(false);
	Process::ptr NewProcess(PseudoConsole *console, std::wstring command_line) noexcept(false);
	CONSOLE_SCREEN_BUFFER_INFO GetCursorInfo(HANDLE console);
	bool CheckStdOut(std::wstring message);
	bool SetupConsoleHost();
	std::string Setup_Console_Stdout();
	std::string Setup_Console_Stdin();
	std::vector<std::wstring>::iterator Split_String(std::wstring_view, char, std::vector<std::wstring>*);
	/**
	 * Create a stdin and stdout pair based on fstreams.
	 * This causes the stdin and stdout to be eaten basically.
	 */
	std::pair<std::fstream, std::fstream> Rebind_Std_In_Out();
}
