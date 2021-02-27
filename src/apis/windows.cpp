#include "apis/alias.hpp"
#ifdef CONPTY_DEBUG
#include <conpty-static.h>
#include <conpty.h>
#endif // CONPTY_DEBUG


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
    switch (error) {
        case 0:
            break;
        case ERROR_OPERATION_ABORTED: {
            throw IO_Operation_Aborted();
        }
        case ERROR_NOT_FOUND:
            throw Not_Found();
        default:
            throw WindowsError(error_message + ", error: " + std::to_string(error));
    }
}

void Alias::check_and_throw_error() noexcept(false) {
    DWORD error = GetLastError();
    if(error != 0) {
        check_and_throw_error(HRESULT_FROM_WIN32(error));
    }
}

auto Alias::CreateStartupInfoForConsole(PseudoConsole* console) noexcept(false) -> STARTUPINFOEXW {
    // TODO Catch and throw errors throughout this block
    SetLastError(0);
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
    startup_info.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(attrListSize));

    check_and_throw_error();
    // Initialize thread attribute list
    if((startup_info.lpAttributeList != nullptr) &&
       (InitializeProcThreadAttributeList(startup_info.lpAttributeList, 1, 0, &attrListSize) != 0)) {
        check_and_throw_error();
        // Set Pseudo Console attribute
        UpdateProcThreadAttribute(startup_info.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                  console->pseudo_console_handle, sizeof(HPCON), nullptr, nullptr);
        check_and_throw_error();

    } else {
        check_and_throw_error();
    }
    // SetLastError(0);

    return startup_info;
}

auto Alias::GetCursorInfo(HANDLE console) -> CONSOLE_SCREEN_BUFFER_INFO {
    CONSOLE_SCREEN_BUFFER_INFO cursor{};
    if(GetConsoleScreenBufferInfo(console, &cursor) == 0) {
        // check_and_throw_error("Couldn't get cursor info");
    }
    return cursor;
}

void Alias::Apply_On_Split_String(std::string_view string_to_split, char delimeter, std::function<void(std::string&)> apply_function) {
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
    if(string_to_split.empty()) {
        return;
    }
    size_t start_pos = 0;
    do {
        auto i = string_to_split.find_first_of(delimeter, start_pos);

        if(i == std::string::npos) {
            i = string_to_split.size() - 1;
        }
        std::string line{string_to_split.begin() + start_pos, string_to_split.begin() + i + 1};
        apply_function(line);

        start_pos = i + 1;
    } while(start_pos != string_to_split.size());
}

auto Alias::WriteToStdOut(std::string message) -> bool {
    auto* std_out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD bytes_written = 0;
    auto results = (bool)WriteFile(std_out, message.c_str(), message.size() * sizeof(char), &bytes_written, nullptr);
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
    WriteToStdOut("\x1b[?1049l");
    SetConsoleMode(hPrimaryOut, stdout_console_mode);
    SetConsoleMode(hPrimaryIn, stdin_console_mode);

    return true;
}

auto Alias::Setup_Console_Stdout(HANDLE hPrimaryConsole) noexcept(false) -> std::string {
    // See https://docs.microsoft.com/en-us/windows/console/setconsolemode for
    // the flag meaning and defaults
    std::string error_message{};
    DWORD console_mode = 0;

    WriteToStdOut("\x1b[?1049h");
    if(!static_cast<bool>(GetConsoleMode(hPrimaryConsole, &stdout_console_mode))) {
        error_message += "Couldn't get console mode for stdout, going to try "
                         "to set it anyway.";
    }
    if(SetConsoleMode(hPrimaryConsole, (ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN)
                      & ~ENABLE_WRAP_AT_EOL_OUTPUT
                      ) == 0) {
        error_message += "Couldn't set console mode for stdin.";
    }
    GetConsoleMode(hPrimaryConsole, &console_mode);
    WriteToStdOut("\x1b[?1049h");
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
    if(SetConsoleMode(hPrimaryConsole, (ENABLE_WINDOW_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT) &
                                       ~ENABLE_PROCESSED_INPUT & ~ENABLE_LINE_INPUT & ~ENABLE_ECHO_INPUT) == 0) {
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

void Alias::Cancel_IO_On_StdOut() {
    CancelIoEx(GetStdHandle(STD_INPUT_HANDLE), nullptr);
    CancelIoEx(GetStdHandle(STD_OUTPUT_HANDLE), nullptr);
}

void Alias::Reset_StdHandles_To_Real() {
    auto conout = CreateFile(L"CONOUT$", GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    SetStdHandle(STD_OUTPUT_HANDLE, conout);

    auto conin = CreateFile(L"CONIN$", GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    SetStdHandle(STD_INPUT_HANDLE, conin);
}
auto Alias::Get_Terminal_Size() -> std::pair<short, short> {
    auto conout = CreateFile(L"CONOUT$", GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    auto terminal_info = Alias::GetCursorInfo(conout);
    return std::make_pair(terminal_info.dwSize.X, terminal_info.dwSize.Y);
}