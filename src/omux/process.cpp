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
    
    Process::Process(Console::Sptr host_in, std::wstring path, std::wstring args)
    : host(host_in), path(path), args(args) {
        command_log.open(L"command_pty."+args+L".log", std::ios_base::out);
        this->process = std::unique_ptr<Alias::Process>(Alias::NewProcess(host->pseudo_console.get(), path + args));
        this->host->process_attached(this);
        saved_cursor_pos = std::string{"\x1b[" + std::to_string(host->layout.y) + ";" + std::to_string(host->layout.x) + "H"};
        this->output_thread = std::thread(&Process::process_output, this);
        
    }
    Process::Process(Console::Sptr host_in) : host(host_in), path(L""), args(L"") {
        this->host->process_attached(this);
    }
    Process::~Process() {
        if(output_thread.joinable()) {
            output_thread.join();
        }
        this->host->process_dettached(this);
    }

    std::string get_repaint_sequence(const Layout& layout) {
        std::stringstream repaint;
        // Erase the buffer by going to the start of line
        // erasing the characters and moving up
        repaint << "\x1b[" << layout.x << "G";
        for(int i = 0; i < layout.height; i++) {
            repaint << "\x1b[" << layout.width << "X\x1b[1F";
            repaint << "\x1b[" << layout.x << "G";
        }
        repaint << "\x1b[" << layout.width << "X";
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
        if(*start == '\x1b') {
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
        // Ensure the x offset is adhered to
        this->host->get_primary_console()->write_to_stdout(sequence);
        if(sequence.back() == 'H') {
            auto pre_offset_cursor = host->pseudo_console->get_cursor_position_as_pair();
            auto corrected_column = pre_offset_cursor.first + host->layout.x;
            auto corrected_row = pre_offset_cursor.second + host->layout.y;
            host->get_primary_console()->write_to_stdout("\x1b[?25h");
            // The absolute movement sequences are relative to the psuedoconsole, so we need to ensure the global offset is applied
            host->get_primary_console()->write_to_stdout("\x1b[" + std::to_string(corrected_row) + ";" + std::to_string(corrected_column) + "H");
        }
        auto cursor_end = host->pseudo_console->get_cursor_position_as_pair();
        return std::make_pair(cursor_end.first - cursor_start.first, cursor_end.second - cursor_start.second);
    }
    /**
    * Deletes length-n characters from a string that aren't part of a control sequence.
    * n > 0 will find n renderable characters from the front of the string and delete the rest.
    * n < 0 will find n renderable characters from the back of the string and up to there.
    * @return the changed line
    */
    auto Process::delete_n_renderable_characters_from_string(std::string& line, int count) -> std::string {
        int n = count;
        if(n > 0) {
            auto start = line.begin();
            auto last_renderable_char = line.begin();
            while(start != line.end() && n > 1) {

                if(*start == '\x1b') {
                    // Everything after ? in ascii is a character that ends a control sequence (maybe)
                    while(*start == '[' || *start == ']' ||
                          static_cast<unsigned int>(*start) <= static_cast<unsigned int>('?')) {
                        start++;
                    }
                    start++;
                } else if(*start == '\r'){
                    n = count;
                    start++;
                    //n--;
                } else {
                    if(n > 1) {

                        n--;
                        last_renderable_char = start;
                        start++;
                    }
                    if(n == 1){
                        start = line.erase(start, line.end());
                    }
                }
            }
            // This happens when the place desired by n is larger than there are renderable characters in the string.
            // So we need to fill the last parts with spaces.
            if(n > 1) {
                line.erase(++last_renderable_char, line.end());
                line.append(n-1, ' ');
            }
        }
        return line;
    }

    auto Process::handle_csi_sequence(std::string_view::iterator& start, std::string_view::iterator& end)
    -> std::string_view::iterator {
        auto control_seq_end = get_control_sequence_end(start, end);
        if(control_seq_end != start) {
            auto sequence = std::string_view{start, control_seq_end};
            // Re-interpret reset control sequence as movement to origin
            if(sequence.compare("\x1b[H") == 0) {
                std::string origin_movement{"\x1b[" + std::to_string(host->layout.y) + ";" + std::to_string(host->layout.x) + "H"};
                host->get_primary_console()->write_to_stdout(origin_movement);
                this->host->scroll_buffer.back().append(origin_movement);
                return control_seq_end;
            }
            auto cursor_movement_diff = track_cursor_for_sequence(sequence);
            
            
            
            // Manage line jumping
            if(cursor_movement_diff.second > 0) {
                // host->scroll_buffer.back().push_back(char_out);
                for(int i = 0; i < cursor_movement_diff.second; i++) {
                    host->scroll_buffer.back().append("\r\n");
                    host->scroll_buffer.push_back(std::string{});
                    host->get_primary_console()->write_to_stdout("\x1b[" + std::to_string(host->layout.x) + "G");
                }
            } else if(cursor_movement_diff.first < 0 || cursor_movement_diff.second < 0) {
                auto cursor = host->pseudo_console->get_cursor_position_as_pair();
                auto* buffer = host->get_scroll_buffer();
                
                
                if(cursor_movement_diff.second != 0) {
                    // When we are moving up lines, we need to delete the lines in the scoll buffer
                    auto line_row_erase_offset = std::min(buffer->size(), static_cast<size_t>(std::abs(cursor_movement_diff.second)));
                    buffer->erase(buffer->end() - line_row_erase_offset, buffer->end());
                    if(!buffer->empty()) {
                        auto& line = buffer->back();
                        // then ensure we are in the right position on the line
                        auto line_position_erase_offset = std::min(line.size(), static_cast<size_t>(cursor.first - host->get_layout().x)) - 1;
                        line.erase(line.begin() + line_position_erase_offset, line.end());
                    } else {
                        buffer->push_back(std::string{});
                    }            
                } else if(cursor_movement_diff.first < 0) {
                    auto& line = buffer->back();
                    delete_n_renderable_characters_from_string(line, (cursor.first));
                   // line.erase(line.begin() + (cursor.first - 1), line.end());
                }
                
                
            } else if(sequence.back() == 'H' && cursor_movement_diff.first > 0 && cursor_movement_diff.second == 0) {
                // An absolute movement sequence has been used to perform an a column movement
                // this should be a relative movement on the line, not an absolute one so we fix it here
                this->host->scroll_buffer.back().append(std::string{"\x1b[" + std::to_string(cursor_movement_diff.first) + "C"});
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
    void Process::output_line_from_scroll_buffer(std::string& output, std::ostream& line) {
        auto start = output.begin();
        auto end = output.end();
        
        while(start != end) {
            auto char_out = *start;
            switch (char_out) {
                case '\n':
                    line << "\n\x1b[" << host->layout.x << "G";
                    break;
                case '\r': {
                    line << "\x1b[" << host->layout.x << "G";
                    break;
                }
                default: {
                   // this->host->get_primary_console()->write_character_to_stdout(*start);
                    line << char_out;
                }      
            }
            start++;
        }
    }
    void Process::set_line_in_screen(unsigned int new_line_in_screen) {
        if(new_line_in_screen > host->layout.height+host->layout.y) {
            auto repaint = get_repaint_sequence(host->layout);
            //this->host->get_primary_console()->write_to_stdout(repaint);
            auto start = host->scroll_buffer.end() - std::min(host->scroll_buffer.size()-1, static_cast<size_t>(host->layout.height));
          //  start++; // As we have gone to the top of screen buffer, we advance one line so it has space to draw the new line coming
            auto end = host->scroll_buffer.end();

            //std::stringstream line;
            std::ostream& line = std::cout;

            line << repaint;
            //line << "\x1b[?12l\x1b[?25l";
            line << "\x1b[?12h\x1b[?25h";
            while(start != end) {
                output_line_from_scroll_buffer(*start, line);
                command_log << *start;
                start++;
            }
            command_log.flush();
            line << "\x1b[?12h\x1b[?25h";
           // host->get_primary_console()->write_to_stdout(line.str());
            line_in_screen = host->layout.height;
        } else {
            line_in_screen = new_line_in_screen;
        }
    }

    void Process::process_string_for_output(std::string_view output) {
        command_log << output;
        
        this->host->get_primary_console()->write_to_stdout(saved_cursor_pos);

        auto start = output.begin();
        auto end = output.end();
        while(start != end) {
            char char_out = *start;
            switch(char_out) {
                case '\r': {
                    // Ensure the origin is shifted about any newlines or carriage returns
                    std::string command{"\r\x1b[" + std::to_string(host->layout.x) + "G"};
                    command_log << command;
                    this->host->get_primary_console()->write_to_stdout(command);
                    // Put the character directly in the scroll buffer as it will handle origin shifting again
                    host->scroll_buffer.back().push_back(char_out);
                    characters_from_start = host->layout.x;

                    break;
                }
                case '\n': {
                    set_line_in_screen(line_in_screen + 1);
                    // Same as carriage return but new line needs to create a new line in the scroll buffer
                    std::string command{"\n\x1b[" + std::to_string(host->layout.x) + "G"};
                    command_log << command;
                    this->host->get_primary_console()->write_to_stdout(command);
                    host->scroll_buffer.back().push_back(char_out);
                    host->scroll_buffer.push_back(std::string{});

                    
                    characters_from_start = host->layout.x;
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
                case '\b': {
                    if(!host->get_scroll_buffer()->back().empty()) {
                        characters_from_start--;
                        auto& line = host->get_scroll_buffer()->back();
                        this->delete_n_renderable_characters_from_string(line, characters_from_start);
                        
                        this->host->get_primary_console()->write_character_to_stdout(char_out);
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
        saved_cursor_pos = host->pseudo_console->get_cursor_position_as_movement();
        command_log.flush();
        //this->host->get_primary_console()->unlock_stdout();
    }

    void Process::process_resize(std::string_view output) {
        // A resize causes a repaint, so we just erase that far in the buffer and let it be re-written in.
        auto start = host->scroll_buffer.end() - std::min(host->scroll_buffer.size(), static_cast<size_t>(host->layout.height));
        host->scroll_buffer.erase(start, host->scroll_buffer.end());
        // If we clear everything, I.E we haven't scrolled yet, we need to ensure there is still something in the buffer
        if(host->scroll_buffer.empty()) {
            host->scroll_buffer.push_back("");
        }
        process_string_for_output(output);
    }

    void Process::process_output() {
        auto* pseudo_console = host->pseudo_console.get();
        std::future<std::string> output_future;
        try {
            output_future = pseudo_console->read_output();
            while(!this->process->stopped()) {
                auto result = output_future.wait_for(std::chrono::milliseconds(16));
                if( result == std::future_status::ready) {
                    std::scoped_lock lock(*this->host->get_primary_console()->get_stdout_lock());
                    if(resize_on_next_output_flag) {
                        resize_on_next_output_flag = false;
                        process_resize(output_future.get());
                    } else {
                        process_string_for_output(output_future.get());
                    }
                    output_future = pseudo_console->read_output();
                }
            }

            // Do this to ensure any lingering blocking reads are cancelled in the future above
            pseudo_console->close_pipes();
        } catch(Alias::IO_Operation_Aborted& e) {
           // host->process_dettached(this);
        } catch(Alias::Not_Found e) {
        }
        host->process_dettached(this);
        
    }
    auto Process::wait_for_idle(int timeout) -> Alias::WAIT_RESULT {
        return this->process->wait_for_idle(timeout);
    }
    auto Process::wait_for_stop(int timeout) -> Alias::WAIT_RESULT {
        return this->process->wait_for_stop(timeout);
    }
    auto Process::process_running() -> bool {
        return !this->process->stopped();
    }
    void Process::resize_on_next_output(Layout old_layout) {
        // This will be the last chance we have access to the existing layout
        // so we need to clear the screen now
        std::scoped_lock lock(*this->host->get_primary_console()->get_stdout_lock());
        host->get_primary_console()->write_to_stdout(get_repaint_sequence(old_layout));
        resize_on_next_output_flag = true;
        saved_cursor_pos = std::string{"\x1b[" + std::to_string(host->layout.y) + ";" + std::to_string(host->layout.x) + "H"};

    }
} // namespace omux