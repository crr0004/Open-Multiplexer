#include "apis/alias.hpp"
#include "omux/console.hpp"
#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <regex>
#include <thread>

namespace omux {
    std::fstream command_log;
    Process::Process(Console::Sptr host_in, std::wstring path, std::wstring args)
    : host(host_in), path(path), args(args) {
        command_log.open("command_pty.log", std::ios_base::out);
        this->process = std::unique_ptr<Alias::Process>(
        Alias::NewProcess(host->pseudo_console.get(), path + args));
        this->host->process_attached(this);
        this->output_thread = std::thread(&Process::process_output, this);
    }
    Process::Process(Console::Sptr host_in)
    : host(host_in), path(L""), args(L"") {
    }
    Process::~Process() {
        if(output_thread.joinable()) {
            output_thread.join();
        }
    }

    auto Process::get_starting_point_for_write(std::vector<std::string>* buffer,
                                               std::vector<std::string>::iterator start,
                                               unsigned int cursor_row)
    -> std::vector<std::string>::iterator {
        auto height_left = host->layout.height - cursor_row;
        auto height = host->layout.height;
        auto end = buffer->end();
        if(end - start > height_left) {
            return (end - (height));
        }
        return start;
    }

    void Process::process_output() {
        // TODO Refactor this code to use futures
        // TODO Refactor this code to use system level handlers/signals to
        // handle process stops
        // TODO Refactor to use scoped locks for primary consoles stdout locking
        auto* pseudo_console = host->pseudo_console.get();
        auto primary_console = host->get_primary_console();
        std::string cursor_pos{"\x1b[" + std::to_string(host->layout.y) + ";" +
                               std::to_string(host->layout.x) + "H"};

        std::stringstream repaint;
        // Erase the buffer by going to the start of line
        // erasing the characters and moving up
        for(int i = 0; i < host->layout.height; i++) {
            repaint << "\x1b[" << std::to_string(host->layout.x + 1) << "G"
                    << "\x1b[" << std::to_string(host->layout.width) << "X\x1b[F";
        }
        std::string reset_paint{repaint.str()};

        while((!this->process->stopped() || pseudo_console->bytes_in_read_pipe() > 0)) {
            /*
            We need to guard against reading an empty pipe
            because windows doesn't support read timeout for pipes
            The ReadFile function that backs the API blocks when no data is
            avaliable and pipes can't have a timeout assiocated with them for
            REASONS (SetCommTimeout just throws error)
            */
            if(pseudo_console->bytes_in_read_pipe() > 0) {
                auto start = pseudo_console->read_output();
                auto* output_buffer = pseudo_console->get_output_buffer();
                auto end = pseudo_console->get_output_buffer()->end();

                primary_console->lock_stdout();
                // Make sure we're in the right frame before continuing
                // primary_console->write_to_stdout(cursor_pos);

                // Output is too big for the buffer, we need to only output the
                // last part depending on the height
                auto layout_height = host->layout.height;
                if(end - start > layout_height) {
                    // primary_console->write_to_stdout(reset_paint);
                    // Grab the new output for scrolling and output it to the
                    // console Use the unbuffered version so it isn't stored
                    // primary_console->write_to_stdout(pseudo_console->read_unbuffered_output());
                    // start = end - layout_height;
                }
                // auto buffer_length = end - start;
                while(start != end) {
                    const auto output = *start;

                    if(this->host->layout.x > 0) {
                        std::string move_to_column{
                        "\x1b[" + std::to_string(host->layout.x) + "G"};
                        primary_console->write_to_stdout(move_to_column);
                    }

                    primary_console->write_to_stdout(output);
                    const auto cursor = pseudo_console->get_cursor_position_as_pair();
                    // We are about to overrun the pane, so we need to start
                    // scrolling
                    if(cursor.second >= layout_height) {
                        command_log << "repaint\n";
                        // primary_console->write_to_stdout(reset_paint);
                        /*
                        if (output_buffer->size() > layout_height) {
                            start -= layout_height;
                        }
                        else {
                            start = output_buffer->begin();
                        }
                        */
                    }
                    start++;
                }

                cursor_pos = Alias::PseudoConsole::get_cursor_position_as_movement();
                primary_console->unlock_stdout();
            } else {
                std::this_thread::sleep_for(
                std::chrono::milliseconds(Alias::OUTPUT_LOOP_SLEEP_TIME_MS));
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
} // namespace omux