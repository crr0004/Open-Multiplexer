#include "omux/console.hpp"
#include "apis/alias.hpp"
#include <chrono>
#include <thread>
#include <memory>
#include <algorithm>
#include <mutex>

namespace omux {
	Process::Process(Console::Sptr host_in, std::wstring path, std::wstring args)
		: host(host_in), path(path), args(args) {
		this->process = Alias::NewProcess(
			host->pseudo_console.get(),
			path + args
		);
		this->host->process_attached(this);
		this->output_thread = std::thread(&Process::process_output, this);
	}
	Process::~Process() {
		if (output_thread.joinable()) {
			output_thread.join();
		}
	}
	void Process::process_output() {
		// TODO Refactor this code to use futures
		// TODO Refactor this code to use system level handlers/signals to handle process stops
		// TODO Refactor to use scoped locks for primary consoles stdout locking
		auto* pseudo_console = host->pseudo_console.get();
		auto primary_console = host->get_primary_console();
		std::string cursor_pos = "\x1b[" + std::to_string(host->layout.y) + ";" + std::to_string(host->layout.x) + "H";
		while (
			(!this->process->stopped() ||
				pseudo_console->bytes_in_read_pipe() > 0)
			) {
			/*
			We need to guard against reading an empty pipe
			because windows doesn't support read timeout for pipes
			The ReadFile function that backs the API blocks when no data is avaliable
			and pipes can't have a timeout assiocated with them for REASONS (SetCommTimeout just throws error)
			*/
			if (pseudo_console->bytes_in_read_pipe() > 0) {
				auto start = pseudo_console->read_output();
				auto end = pseudo_console->get_output_buffer()->end();
				
				primary_console->lock_stdout();

				/*
				To account for positive (i.e cursor moving down the screen) scrolling we need to deal with three cases of output and screen buffer sizes.
				We do this here because the primary console only cares about the whole screen buffer
				where as the pseudoconsole and processes care about their subsection of the screen buffer.

				The three cases are:
					- There is enough space to write the output
					- Our subsection needs to be scrolled less than 100% to fit the output
					- There is not enough space regardless if we scroll (think cating a file)
				To account for these three cases we need to move the cursor and output iterator to the correct point.
				*/

				primary_console->write_to_stdout(cursor_pos);
				while (start != end) {
					auto output = *start;
					if (this->host->layout.x > 0) {
						std::string move_to_column{ "\x1b[" + std::to_string(host->layout.x) + "G" };
						primary_console->write_to_stdout(move_to_column);
					}
					primary_console->write_to_stdout(output);
					start++;
				}
				cursor_pos = Alias::PseudoConsole::get_cursor_position_as_movement();
				primary_console->unlock_stdout();
			}
			else {
				std::this_thread::sleep_for(
					std::chrono::milliseconds(Alias::OUTPUT_LOOP_SLEEP_TIME_MS)
				);
			}
		}
		host->process_dettached(this);
	}
	void Process::wait_for_stop(unsigned long timeout) {
		this->process->wait_for_stop(timeout);
	}
	auto Process::process_running() -> bool {
		return !this->process->stopped();
	}
}