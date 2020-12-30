#include "apis/alias.hpp"
#include <conpty-static.h>
#include <conpty.h>
#include <fcntl.h>
#include <io.h>
#include <string_view>


/**
 * Welcome to the mess of the Windows API usage for creating and managing PTY
 *and processes.
 *
 * Some of the functions (Creation should really be in constructors)
 * need refactoring so feel free to do that.
 *
 * Ideally the PseudoConsole and Process classes are platform independant
 * and the alias header just forward declares the functions,
 * and each platform has its own extra header for more functions.
 * The compile system would then link each implementation as need be.
 *
 * So far I have only implemented Windows so I am not really sure HOW that
 * abstraction would actually play out in code.
 *
 **/
void Alias::check_and_throw_error(HRESULT error) noexcept(false) {
    if(error != S_OK) {
        throw WindowsError(error);
    }
}
void Alias::check_and_throw_error(std::string error_message) noexcept(false) {
    DWORD error = GetLastError();
    if(error != 0) {
        throw WindowsError(error_message + ", error: " + std::to_string(error));
    }
}
void Alias::check_and_throw_error() noexcept(false) {
    DWORD error = GetLastError();
    if(error != 0) {
        check_and_throw_error(HRESULT_FROM_WIN32(error));
    }
}
auto Alias::CreatePseudoConsole(int x, int y, int columns, int rows) noexcept(false)
-> Alias::PseudoConsole::ptr {
    SetLastError(0);
    HRESULT hr{E_UNEXPECTED};
    HANDLE pty_stdin_pipe{INVALID_HANDLE_VALUE};
    HANDLE pty_stdout_pipe{INVALID_HANDLE_VALUE};
    HANDLE pipe_write_handle{INVALID_HANDLE_VALUE};
    HANDLE pipe_read_handle{INVALID_HANDLE_VALUE};
    HPCON pseudo_console_handle{};

    // Create the pipes to which the ConPTY will connect
    CreatePipe(&pty_stdin_pipe, &pipe_write_handle, nullptr,
               Alias::READ_BUFFER_SIZE * sizeof(char));
    CreatePipe(&pipe_read_handle, &pty_stdout_pipe, nullptr,
               Alias::READ_BUFFER_SIZE * sizeof(char));
    check_and_throw_error();
    // Determine required size of Pseudo Console
    check_and_throw_error();
    COORD consoleSize{columns, 80};

    // Create the Pseudo Console of the required size, attached to the PTY-end
    // of the pipes
    hr = CreatePseudoConsole(consoleSize, pty_stdin_pipe, pty_stdout_pipe, 1,
                             &pseudo_console_handle);
    if(hr == S_OK) {

        // Note: We can close the handles to the PTY-end of the pipes here
        // because the handles are dup'ed into the ConHost and will be released
        // when the ConPTY is destroyed.
        // CloseHandle(pty_stdout_pipe);
        CloseHandle(pty_stdin_pipe);
        SetLastError(0);

        return std::make_unique<PseudoConsole>(x, y, pseudo_console_handle, pipe_write_handle,
                                               pipe_read_handle, pty_stdout_pipe);
    }
    SetLastError(0);
    throw WindowsError(
    "Something went wrong in creating pseudo console error: " + std::to_string(hr));
}
auto Alias::CreateStartupInfoForConsole(PseudoConsole* console) noexcept(false) -> STARTUPINFOEXW {
    // TODO Catch and throw errors throughout this block
    SetLastError(0);
    check_and_throw_error();
    STARTUPINFOEXW startup_info{};
    SIZE_T attrListSize{0};

    startup_info.StartupInfo.cb = sizeof(STARTUPINFOEX);
    /*
    startup_info.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup_info.StartupInfo.hStdOutput = console->pipe_out;
    startup_info.StartupInfo.hStdError = console->pipe_out;
    startup_info.StartupInfo.hStdInput = console->pipe_in;
    */

    // Get the size of the thread attribute list.
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
    // Clear the error because this functions always returns as error as per
    // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-initializeprocthreadattributelist#remarks
    SetLastError(0);


    // Allocate a thread attribute list of the correct size
    startup_info.lpAttributeList =
    reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(attrListSize));

    check_and_throw_error();
    // Initialize thread attribute list
    if((startup_info.lpAttributeList != nullptr) &&
       (InitializeProcThreadAttributeList(startup_info.lpAttributeList, 1, 0,
                                          &attrListSize) != 0)) {
        check_and_throw_error();
        // Set Pseudo Console attribute
        UpdateProcThreadAttribute(
        startup_info.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        console->pseudo_console_handle, sizeof(HPCON), nullptr, nullptr);
        check_and_throw_error();

    } else {
        check_and_throw_error();
    }
    // SetLastError(0);

    return startup_info;
}
auto Alias::NewProcess(PseudoConsole* console, std::wstring command_line) noexcept(false)
-> Alias::Process* {
    auto startupInfo = CreateStartupInfoForConsole(console);
    PROCESS_INFORMATION piClient{};

    if(CreateProcessW(
       // cmd.data(),                           // No module name - use Command
       // Line
       nullptr,
       (wchar_t*)command_line.data(), // Command Line
       nullptr, // Process handle not inheritable
       nullptr, // Thread handle not inheritable
       FALSE, // Inherit handles
       EXTENDED_STARTUPINFO_PRESENT, // Creation flags
       nullptr, // Use parent's environment block
       nullptr, // Use parent's starting directory
       &(startupInfo.StartupInfo), // Pointer to STARTUPINFO
       &piClient) == 0) // Pointer to PROCESS_INFORMATION
    {
        check_and_throw_error(std::string{"Couldn't create process "} +
                              (char*)command_line.c_str());
    };

    auto process = new Alias::Process{startupInfo, piClient};
    console->process_attached(process);
    return process;
}
auto Alias::GetCursorInfo(HANDLE console) -> CONSOLE_SCREEN_BUFFER_INFO {
    CONSOLE_SCREEN_BUFFER_INFO cursor{};
    if(GetConsoleScreenBufferInfo(console, &cursor) == 0) {
        // check_and_throw_error("Couldn't get cursor info");
    }
    return cursor;
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
        if(WriteFile(this->pipe_in, cursor_pos.data(), cursor_pos.size() * sizeof(char),
                     &bytes_written, nullptr) == false) {
            check_and_throw_error("Failed to write cursor pos to console");
        }
        // output_buffer.pop_back();
        // We don't want the cursor position request existing in the buffer
    }

    // this->read_output();
    // output_buffer.clear();
    //
    // output_buffer.push_back(
    //{ "\x1b[" + std::to_string(y) + ";" + std::to_string(x) + "H" }
    //);
}
auto Alias::Split_String(std::string_view string_to_split,
                         char delimeter,
                         std::vector<std::string>* buffer)
-> std::vector<std::string>::iterator {
    size_t index_of_first_new_string = buffer->size();
    // Don't want to cause an underflow with the -1
    if(buffer->empty()) {
        index_of_first_new_string = 0;
    }
    /**
     * I know this seems bizare, but what it does is split the read buffer based
     * on new lines. However there are two extra things we need to account for.
     * We want the new line in the string split, and we want the last section of
     * output even if it doesn't contain a new line.
     *
     * To include the new line we need to +1 to the start position and length.
     * To include the last section of output, we need to substr with the
     * bytes_read, which is the rest of the read string. We also use bytes_read
     * to break the loop.
     */
    size_t i = 0;
    size_t start_pos = 0;
    size_t length = 0;
    do {
        i = string_to_split.find_first_of(delimeter, start_pos);
        length = i + 1 - start_pos;
        if(i == std::string::npos) {
            length = std::string::npos;
        }
        auto string_to_copy = string_to_split.substr(start_pos, length);
        buffer->push_back(std::string{string_to_copy.begin(), string_to_copy.end()});
        start_pos = i + 1;
        // Account for two ending conditions.
        // The string ends with new line so we check the +1 goes to buffer size.
        // The string doesn't end with a new line so we substr the rest and i is
        // npos
    } while(start_pos != string_to_split.size() && i != std::string::npos);
    return buffer->begin() + index_of_first_new_string; // Want to move the
                                                        // iterator to first
                                                        // instance of the new
                                                        // strings
}
auto Alias::PseudoConsole::read_output() -> Alias::PseudoConsole::BufferIterator {
    std::string chars(Alias::READ_BUFFER_SIZE, '\0');
    size_t index_of_first_new_string = output_buffer.size();
    if(output_buffer.empty()) {
        index_of_first_new_string = 0;
    }
    DWORD bytes_read = 0;
    do {
        if(!static_cast<bool>(ReadFile(this->pipe_out, chars.data(),
                                       Alias::READ_BUFFER_SIZE * sizeof(char),
                                       &bytes_read, nullptr))) {
            check_and_throw_error("Failed to read from console");
        }
        std::string_view string_to_split{chars.data(), bytes_read / sizeof(char)};
        Alias::Split_String(string_to_split, (char)'\n', &output_buffer);

    } while(this->bytes_in_read_pipe() > 0);

    this->last_read_in = chars.substr(0,
                                      bytes_read / sizeof(char)); // Keep a copy
                                                                  // of the last
                                                                  // read so we
                                                                  // can test
                                                                  // with it.
    return output_buffer.begin() + index_of_first_new_string; // Want to move
                                                              // the iterator to
                                                              // first instance
                                                              // of the new
                                                              // strings
}
auto Alias::PseudoConsole::read_unbuffered_output() -> std::string {
    std::string chars(Alias::READ_BUFFER_SIZE, '\0');
    std::string output;

    DWORD bytes_read = 0;
    do {
        if(!static_cast<bool>(ReadFile(this->pipe_out, chars.data(),
                                       Alias::READ_BUFFER_SIZE * sizeof(char),
                                       &bytes_read, nullptr))) {
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
    cursor_pos << "\x1b[" << x << ";" << y << "R";
    return std::string{cursor_pos.str()};
}
auto Alias::PseudoConsole::get_cursor_position_as_movement() -> std::string {
    HANDLE stdout_handle{GetStdHandle(STD_OUTPUT_HANDLE)};
    auto cursor_info = Alias::GetCursorInfo(stdout_handle);
    std::stringstream cursor_pos{};
    cursor_pos << "\x1b[" << cursor_info.dwCursorPosition.Y + 1 << ";"
               << cursor_info.dwCursorPosition.X + 1 << "H";
    return std::string{cursor_pos.str()};
}
auto Alias::PseudoConsole::get_cursor_position_as_pair()
-> std::pair<unsigned int, unsigned int> {
    HANDLE stdout_handle{GetStdHandle(STD_OUTPUT_HANDLE)};
    auto cursor_info = Alias::GetCursorInfo(stdout_handle);
    return std::make_pair(cursor_info.dwCursorPosition.X + 1,
                          cursor_info.dwCursorPosition.Y + 1);
}
void Alias::PseudoConsole::write_input(std::string_view input) const {
    DWORD bytes_written = 0;
    if(!static_cast<bool>(WriteFile(this->pipe_in, input.data(), input.size() * sizeof(char),
                                    &bytes_written, nullptr))) {
        check_and_throw_error("Couldn't write to pipe_in");
    }
}
void Alias::PseudoConsole::write_to_pty_stdout(std::string_view input) const {
    DWORD bytes_written = 0;
    if(!static_cast<bool>(WriteFile(this->pty_pipe_out, input.data(),
                                    input.size() * sizeof(char), &bytes_written, nullptr))) {
        check_and_throw_error("Couldn't write to pipe_in");
    }
}

auto Alias::Process::stopped() const -> bool {
    DWORD exit_code = 0;
    GetExitCodeProcess(this->process_info.hProcess, &exit_code);
    return exit_code != STILL_ACTIVE;
}
auto Alias::CheckStdOut(std::string message) -> bool {
    auto* std_out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD bytes_written = 0;
    auto results = (bool)WriteFile(std_out, message.c_str(),
                                   message.size() * sizeof(char), &bytes_written, nullptr);
    if(!results) {
        check_and_throw_error("Couldn't write to stdout console. Wrote "
                              "chars: " +
                              std::to_string(bytes_written));
    }
    return results;
}
DWORD stdout_console_mode = 0;
DWORD stdin_console_mode = 0;
auto Alias::SetupConsoleHost() noexcept(false) -> bool {
    auto stdout_error = Alias::Setup_Console_Stdout(GetStdHandle(STD_OUTPUT_HANDLE));
    auto stdin_error = Alias::Setup_Console_Stdin(GetStdHandle(STD_INPUT_HANDLE));
    if(!stdout_error.empty()) {
        check_and_throw_error(stdout_error);
    }
    if(!stdin_error.empty()) {
        check_and_throw_error(stdin_error);
    }
    return true;
}
auto Alias::ReverseSetupConsoleHost() noexcept(false) -> bool {
    HANDLE hPrimaryOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hPrimaryIn = GetStdHandle(STD_INPUT_HANDLE);
    CheckStdOut("\x1b[?1049l");
    SetConsoleMode(hPrimaryOut, stdout_console_mode);
    SetConsoleMode(hPrimaryIn, stdin_console_mode);

    return true;
}
auto Alias::Setup_Console_Stdout(HANDLE hPrimaryConsole) noexcept(false) -> std::string {
    // See https://docs.microsoft.com/en-us/windows/console/setconsolemode for
    // the flag meaning and defaults
    std::string error_message{};
    DWORD console_mode = 0;

    if(!static_cast<bool>(GetConsoleMode(hPrimaryConsole, &stdout_console_mode))) {
        error_message += "Couldn't get console mode for stdout, going to try "
                         "to set it anyway.";
    }
    if(SetConsoleMode(hPrimaryConsole, (stdout_console_mode | ENABLE_PROCESSED_OUTPUT |
                                        ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN) |
                                       ENABLE_WRAP_AT_EOL_OUTPUT
                      //(ENABLE_PROCESSED_OUTPUT |
                      // ENABLE_VIRTUAL_TERMINAL_PROCESSING)
                      ) == 0) {
        error_message += "Couldn't set console mode for stdin.";
    }
    GetConsoleMode(hPrimaryConsole, &console_mode);
    // CheckStdOut("\x1b[?1049h");
    return error_message;
}
auto Alias::Setup_Console_Stdin(HANDLE hPrimaryConsole) noexcept(false) -> std::string {
    // See https://docs.microsoft.com/en-us/windows/console/setconsolemode for
    // the flag meaning and defaults
    std::string error_message{};
    DWORD console_mode = 0;

    if(!static_cast<bool>(GetConsoleMode(hPrimaryConsole, &stdin_console_mode))) {
        error_message += "Couldn't get console mode for stdin, going to try to "
                         "set it anyway.";
    }
    if(SetConsoleMode(hPrimaryConsole,
                      (stdin_console_mode | ENABLE_WINDOW_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT) &
                      ~ENABLE_PROCESSED_INPUT & ~ENABLE_LINE_INPUT) == 0) {
        error_message += "Couldn't set console mode for stdin.";
    }
    GetConsoleMode(hPrimaryConsole, &console_mode);
    return error_message;
}
auto Alias::Rebind_Std_In_Out() noexcept(false) -> std::pair<std::fstream, std::fstream> {
    HANDLE write_pipe = nullptr;
    HANDLE read_pipe = nullptr;
    CreatePipe(&read_pipe, &write_pipe, nullptr, 0);
    SetStdHandle(STD_INPUT_HANDLE, read_pipe);
    SetStdHandle(STD_OUTPUT_HANDLE, write_pipe);
    int out_file_descriptor = _open_osfhandle((intptr_t)write_pipe, 0);
    int in_file_descriptor = _open_osfhandle((intptr_t)read_pipe, 0);

    if(in_file_descriptor != -1 && out_file_descriptor != -1) {
        FILE* out_file = _fdopen(out_file_descriptor, "w");
        FILE* in_file = _fdopen(in_file_descriptor, "r");
        return std::make_pair(std::fstream(in_file), std::fstream(out_file));
    }
    throw WindowsError("Couldn't rebind stdout and stdin to ifstream and "
                       "ofstream, does stdout/stdin exist?");
}
auto Alias::Get_StdIn_As_Stream() -> std::pair<std::ifstream, std::ofstream> {
    HANDLE write_pipe = nullptr;
    HANDLE read_pipe = nullptr;
    CreatePipe(&read_pipe, &write_pipe, nullptr, 0);
    SetStdHandle(STD_INPUT_HANDLE, read_pipe);
    int write_file_descriptor = _open_osfhandle((intptr_t)write_pipe, _O_APPEND);
    int read_file_descriptor = _open_osfhandle((intptr_t)read_pipe, _O_RDONLY);

    if(read_file_descriptor != -1 && write_file_descriptor != -1) {
        FILE* write_file = _fdopen(write_file_descriptor, "w");
        FILE* read_file = _fdopen(read_file_descriptor, "r");
        if(write_file == nullptr || read_file == nullptr) {
            throw WindowsError("Couldn't create files from file descriptors");
        }
        return std::make_pair(std::ifstream(read_file), std::ofstream(write_file));
    }
    throw WindowsError("Couldn't rebindstdin to ifstream and ofstream, does "
                       "stdin exist?");
}
auto Alias::Get_StdOut_As_Stream() -> std::pair<std::ifstream, std::ofstream> {
    HANDLE write_pipe = nullptr;
    HANDLE read_pipe = nullptr;
    CreatePipe(&read_pipe, &write_pipe, nullptr, 0);
    SetStdHandle(STD_OUTPUT_HANDLE, write_pipe);
    int write_file_descriptor = _open_osfhandle((intptr_t)write_pipe, _O_APPEND);
    int read_file_descriptor = _open_osfhandle((intptr_t)read_pipe, _O_RDONLY);

    if(read_file_descriptor != -1 && write_file_descriptor != -1) {
        FILE* write_file = _fdopen(write_file_descriptor, "w");
        FILE* read_file = _fdopen(read_file_descriptor, "r");
        if(write_file == nullptr || read_file == nullptr) {
            throw WindowsError("Couldn't create files from file descriptors");
        }
        return std::make_pair(std::ifstream(read_file), std::ofstream(write_file));
    }
    throw WindowsError("Couldn't rebind stdout to ifstream and ofstream, does "
                       "stdout exist?");
}