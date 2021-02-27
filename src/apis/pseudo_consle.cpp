#include "alias.hpp"
auto Alias::CreatePseudoConsole(int x, int y, short columns, short rows) noexcept(false) -> Alias::PseudoConsole::ptr {
    SetLastError(0);
    HRESULT hr{E_UNEXPECTED};
    HANDLE pty_stdin_pipe{INVALID_HANDLE_VALUE};
    HANDLE pty_stdout_pipe{INVALID_HANDLE_VALUE};
    HANDLE pipe_write_handle{INVALID_HANDLE_VALUE};
    HANDLE pipe_read_handle{INVALID_HANDLE_VALUE};
    HPCON pseudo_console_handle{};

    // Create the pipes to which the ConPTY will connect
    if(CreatePipe(&pty_stdin_pipe, &pipe_write_handle, nullptr, Alias::READ_BUFFER_SIZE * sizeof(char)) == 0) {
        check_and_throw_error();
    }
    if(CreatePipe(&pipe_read_handle, &pty_stdout_pipe, nullptr, Alias::READ_BUFFER_SIZE * sizeof(char)) == 0) {
        check_and_throw_error();
    }    
    // SHORT rows_adjusted = rows + ScreenBufferInfo.dwSize.Y+1;
    COORD consoleSize{columns, rows};

    // Create the Pseudo Console of the required size, attached to the PTY-end
    // of the pipes
    hr = CreatePseudoConsole(consoleSize, pty_stdin_pipe, pty_stdout_pipe, 0
        , &pseudo_console_handle);
    if(hr == S_OK) {

        // Note: We can close the handles to the PTY-end of the pipes here
        // because the handles are dup'ed into the ConHost and will be released
        // when the ConPTY is destroyed.
        CloseHandle(pty_stdout_pipe);
        CloseHandle(pty_stdin_pipe);
        SetLastError(0);

        return std::make_unique<PseudoConsole>(x, y, pseudo_console_handle, pipe_write_handle, pipe_read_handle);
    }
    SetLastError(0);
    throw WindowsError("Something went wrong in creating pseudo console error: " + std::to_string(hr));
}

void Alias::PseudoConsole::process_attached(Alias::Process* process) {
    // When we create the pseudoconsle, it will emit a position request on
    // pipe_out due to submitting PSEUDOCONSOLE_INHERIT_CURSOR when creating the
    // console

    std::string output(4, '\0');
    DWORD bytes_read = 0;
    ReadFile(this->pipe_out, output.data(), output.size(), &bytes_read, nullptr);
    if(bytes_read == 4 && std::string{"\x1b[6n"} == output) {
        auto cursor_pos = this->get_cursor_position_as_vt(this->x, this->y);
        DWORD bytes_written = 0;
        if(WriteFile(this->pipe_in, cursor_pos.data(), cursor_pos.size() * sizeof(char), &bytes_written, nullptr) == false) {
            check_and_throw_error("Failed to write cursor pos to console");
        }
        // output_buffer.pop_back();
        // We don't want the cursor position request existing in the buffer
    }

    // Discard whatever
    // read_unbuffered_output();
    // output_buffer.clear();
    //
    // output_buffer.push_back(
    //{ "\x1b[" + std::to_string(y) + ";" + std::to_string(x) + "H" }
    //);
}
void Alias::PseudoConsole::close_pipes() {
    
    
    CancelIoEx(this->pipe_in, nullptr);
    CancelIoEx(this->pipe_out, nullptr);
    
    CloseHandle(pipe_in);
    CloseHandle(pipe_out);

    pipe_in = 0;
    pipe_out = 0;
    
    SetLastError(0);
}
auto Alias::PseudoConsole::read_output() -> std::future<std::string> {
    return std::async(std::launch::async, [&]() {
        std::string chars(Alias::READ_BUFFER_SIZE, '\0');
        DWORD bytes_read = 0;
        if(pipe_out != 0 && ReadFile(this->pipe_out, chars.data(), Alias::READ_BUFFER_SIZE * sizeof(char), &bytes_read, nullptr) == 0) {
            check_and_throw_error("Failed to read from console");
            
        }
        last_read_in = chars.substr(0, bytes_read / sizeof(char));
        // Want to move the iterator to first instance of the new strings
        return chars.substr(0, bytes_read / sizeof(char));
    });
}

auto Alias::PseudoConsole::read_unbuffered_output() -> std::string {
    std::string chars(Alias::READ_BUFFER_SIZE, '\0');
    std::string output;

    DWORD bytes_read = 0;
    do {
        if(!static_cast<bool>(ReadFile(this->pipe_out, chars.data(), Alias::READ_BUFFER_SIZE * sizeof(char), &bytes_read, nullptr))) {
            check_and_throw_error("Failed to read from console");
        }
        output.append(chars.data(), bytes_read);

    } while(this->bytes_in_read_pipe() > 0);
    return output;
}

auto Alias::PseudoConsole::bytes_in_read_pipe() const -> size_t {
    DWORD bytes_in_pipe = 0;
    if(PeekNamedPipe(this->pipe_out,
                     nullptr, // don't want to actually copy the data
                     0, nullptr, &bytes_in_pipe, nullptr) == 0) {
        check_and_throw_error("Couldn't peek at read_pipe");
    }
    return bytes_in_pipe;
}

auto Alias::PseudoConsole::get_cursor_position_as_vt(int x, int y) -> std::string {
    std::stringstream cursor_pos{};
    cursor_pos << "\x1b[" << y << ";" << x << "R";
    return std::string{cursor_pos.str()};
}

auto Alias::PseudoConsole::get_cursor_position_as_movement() -> std::string {
    HANDLE stdout_handle{GetStdHandle(STD_OUTPUT_HANDLE)};
    auto cursor_info = Alias::GetCursorInfo(stdout_handle);
    std::stringstream cursor_pos{};
    cursor_pos << "\x1b[" << cursor_info.dwCursorPosition.Y + 1 << ";" << cursor_info.dwCursorPosition.X + 1 << "H";
    return std::string{cursor_pos.str()};
}

auto Alias::PseudoConsole::get_cursor_position_as_pair() -> std::pair<unsigned int, unsigned int> {
    HANDLE stdout_handle{GetStdHandle(STD_OUTPUT_HANDLE)};
    auto cursor_info = Alias::GetCursorInfo(stdout_handle);
    return std::make_pair(cursor_info.dwCursorPosition.X + 1, cursor_info.dwCursorPosition.Y + 1);
}

void Alias::PseudoConsole::write_input(std::string_view input) const {
    DWORD bytes_written = 0;
    if(!static_cast<bool>(WriteFile(this->pipe_in, input.data(), input.size() * sizeof(char), &bytes_written, nullptr))) {
        check_and_throw_error("Couldn't write to pipe_in");
    }
}

void Alias::PseudoConsole::resize(short columns, short rows) {
    ResizePseudoConsole(pseudo_console_handle, {columns, rows});
}