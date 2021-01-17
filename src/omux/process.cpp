#include "apis/alias.hpp"
#include "omux/console.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <regex>
#include <thread>

namespace omux {
    std::fstream command_log;
    Process::Process(Console::Sptr host_in, std::wstring path, std::wstring args)
    : host(host_in), path(path), args(args) {
        command_log.open("command_pty.log", std::ios_base::out);
        this->process = std::unique_ptr<Alias::Process>(Alias::NewProcess(host->pseudo_console.get(), path + args));
        this->host->process_attached(this);
        this->output_thread = std::thread(&Process::process_output, this);
    }
    Process::Process(Console::Sptr host_in) : host(host_in), path(L""), args(L"") {
    }
    Process::~Process() {
        if(output_thread.joinable()) {
            output_thread.join();
        }
    }

    std::string get_repaint_sequence(const Layout& layout) {
        std::stringstream repaint;
        // Erase the buffer by going to the start of line
        // erasing the characters and moving up
        repaint << "\x1b[" << std::to_string(layout.x) << "G";
        for(int i = 0; i < layout.height; i++) {
            repaint << "\x1b[" << std::to_string(layout.width) << "X\x1b[1F";
        }
        repaint << "\x1b[" << std::to_string(layout.width) + "X";
        return repaint.str();
    }

    /**
     * Find the end of a control sequence
     * @param start start of the control sequence
     * @param end end of the string view
     * @return end of sequence or start if sequence isn't valid
     */
    auto get_control_sequence_end(std::string_view::iterator start, std::string_view::iterator end) -> std::string_view::iterator {
        auto end_of_sequence = start;
        if(*end_of_sequence == '\x1b') {
            // Everything after ? in ascii is a character that ends a control sequence (maybe)
            while(*end_of_sequence == '[' || *end_of_sequence == ']' ||
                  static_cast<unsigned int>(*end_of_sequence) <= static_cast<unsigned int>('?')) {
                end_of_sequence++;
            }
            // To include the end of sequence character
            end_of_sequence++;
        }
        auto sequence = std::string{start, end_of_sequence};
        return end_of_sequence;
    }

    auto Process::track_cursor_for_sequence(std::string_view sequence) -> std::pair<int, int> {
        auto cursor_start = host->pseudo_console->get_cursor_position_as_pair();
        this->host->get_primary_console()->write_to_stdout(sequence);
        auto cursor_end = host->pseudo_console->get_cursor_position_as_pair();
        return std::make_pair(cursor_end.first - cursor_start.first, cursor_end.second - cursor_start.second);
    }

    auto Process::handle_csi_sequence(std::string_view::iterator& start, std::string_view::iterator& end)
    -> std::string_view::iterator {
        auto control_seq_end = get_control_sequence_end(start, end);
        if(control_seq_end != start) {
            auto sequence = std::string_view{start, control_seq_end};
            if(sequence.compare("\x1b[10;60H") == 0) {
                command_log << sequence;
            }
            auto cursor_movement_diff = track_cursor_for_sequence(sequence);
            // Only really care about lines jumping
            if(cursor_movement_diff.second > 0) {
                // host->scroll_buffer.back().push_back(char_out);
                for(int i = 0; i < cursor_movement_diff.second; i++) {
                    host->scroll_buffer.back().push_back('\n');
                    host->scroll_buffer.push_back(std::string{});
                    host->get_primary_console()->write_to_stdout("\x1b[" + std::to_string(host->layout.x) + "G");
                }
            } else if(sequence.back() == 'H' && cursor_movement_diff.first != 0 && cursor_movement_diff.second == 0) {
                // An absolute movement sequence has been used to perform an a column movement
                // this should be a relative movement on the line, not an absolute one so we fix it here
                auto command = std::string{"\x1b["};
                if(cursor_movement_diff.first < 0) {
                    command.append(std::to_string(std::abs(cursor_movement_diff.first)) + "D");
                } else {
                    command.append(std::to_string(cursor_movement_diff.first) + "C");
                }
                this->host->scroll_buffer.back().append(command);
                // host->scroll_buffer.push_back(std::string{sequence});
            } else if(cursor_movement_diff.second < 0) {
                // Not sure how to handle negative line movement
                // host->scroll_buffer.push_back(std::string{});
            } else if(cursor_movement_diff.second == 0 && cursor_movement_diff.first == 0 && sequence.back() == 'H') {
                // Capital H is the control code for absolute movement. So if nothing happened in the absolute movement
                // then we don't want the sequence in the scroll buffer
                // These seem to appear from the conhost when the max screen buffer line limit is reached
                // host->scroll_buffer.push_back(std::string{});

            } else {
                this->host->scroll_buffer.back().append(sequence);
            }
        }
        return control_seq_end;
    }

    void Process::set_line_in_screen(unsigned int new_line_in_screen) {
        if(new_line_in_screen > host->layout.height) {
            auto repaint = get_repaint_sequence(host->layout);
            this->host->get_primary_console()->write_to_stdout(repaint);
            auto start = host->scroll_buffer.end() - std::min(host->scroll_buffer.size(), static_cast<size_t>(line_in_screen));
            auto end = host->scroll_buffer.end();
            while(start != end) {
                this->host->get_primary_console()->write_to_stdout("\x1b[" + std::to_string(host->layout.x) + "G");
                this->host->get_primary_console()->write_to_stdout(*start);
                start++;
            }
            line_in_screen = host->layout.height;
        } else {
            line_in_screen = new_line_in_screen;
        }
    }

    void Process::process_string_for_output(std::string_view output) {
        command_log << output;
        command_log.flush();

        // TODO process each character of output and put it correctly in the scroll buffer
        auto start = output.begin();
        auto end = output.end();
        while(start != end) {
            char char_out = *start;
            switch(char_out) {
                case '\r': {
                    // Ensure the origin is shifted about any newlines or carriage returns
                    this->host->get_primary_console()->write_to_stdout("\r\x1b[" + std::to_string(host->layout.x) + "G");
                    // Put the character directly in the scroll buffer as it will handle origin shifting again
                    host->scroll_buffer.back().push_back(char_out);
                    characters_from_start = 0;
                    break;
                }
                case '\n': {
                    // Same as carriage return but new line needs to create a new line in the scroll buffer
                    this->host->get_primary_console()->write_to_stdout("\n\x1b[" + std::to_string(host->layout.x) + "G");
                    host->scroll_buffer.back().push_back(char_out);
                    host->scroll_buffer.push_back(std::string{});

                    set_line_in_screen(line_in_screen + 1);
                    characters_from_start = 0;
                    break;
                }
                case '\x1b': {
                    // don't care about OSC commands
                    if(*(start + 1) == ']') {
                        host->scroll_buffer.back().push_back(char_out);
                        this->host->get_primary_console()->write_character_to_stdout(char_out);
                    } else {
                        // Backup one here because we are about to increment but we are already where we want to be
                        start = handle_csi_sequence(start, end) - 1;

                        // control sequences could put us anywhere
                        auto cursor_pos = host->pseudo_console->get_cursor_position_as_pair();
                        set_line_in_screen(cursor_pos.second);
                        characters_from_start = cursor_pos.first;
                    }
                    break;
                }

                default: {
                    host->scroll_buffer.back().push_back(char_out);
                    this->host->get_primary_console()->write_character_to_stdout(char_out);
                    characters_from_start++;
                }
            }
            start++;
        }
    }

    void Process::process_output() {
        auto* pseudo_console = host->pseudo_console.get();

        auto output_future = pseudo_console->read_output();
        do {
            auto result = output_future.wait_for(std::chrono::milliseconds(16));
            if(output_future.valid() && result == std::future_status::ready) {
                process_string_for_output(output_future.get());
                output_future = pseudo_console->read_output();
            }
        } while(!this->process->stopped());
        // Do this to ensure any lingering blocking reads are cancelled in the future above
        pseudo_console->cancel_io_on_pipes();

        host->process_dettached(this);
    }
    void Process::wait_for_stop(unsigned long timeout) {
        this->process->wait_for_stop(timeout);
    }
    auto Process::process_running() -> bool {
        return !this->process->stopped();
    }
} // namespace omux