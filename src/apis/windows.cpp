#include "apis/alias.hpp"
using namespace Alias;
void Alias::check_and_throw_error(HRESULT error) noexcept(false){
    if(error != S_OK){
        throw WindowsError(error);
    }
}
void Alias::check_and_throw_error(std::string error_message) noexcept(false){
    DWORD error = GetLastError();
    if(error != 0){
        throw WindowsError(error_message + ", error: " + std::to_string(error));
    }

}
void Alias::check_and_throw_error() noexcept(false){
    DWORD error = GetLastError();
    if(error != 0){
        check_and_throw_error(HRESULT_FROM_WIN32(error));
    }

}
Alias::PseudoConsole::ptr Alias::CreatePseudoConsole() noexcept(false){
        SetLastError(0);
        HRESULT hr{ E_UNEXPECTED };
        HANDLE hPipePTYIn{ INVALID_HANDLE_VALUE };
        HANDLE hPipePTYOut{ INVALID_HANDLE_VALUE };
        HANDLE phPipeOut{ INVALID_HANDLE_VALUE};
        HANDLE phPipeIn{INVALID_HANDLE_VALUE};
        HPCON pseudoConsoleHandle{};

        // Create the pipes to which the ConPTY will connect
        CreatePipe(&hPipePTYIn, &phPipeOut, NULL, 0);
        CreatePipe(&phPipeIn, &hPipePTYOut, NULL, 0);
        check_and_throw_error();
        // Determine required size of Pseudo Console
        COORD consoleSize{};
        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        HANDLE hConsole{ GetStdHandle(STD_OUTPUT_HANDLE) };
        check_and_throw_error();
        if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
                consoleSize.X = csbi.srWindow.Right - csbi.srWindow.Left + 1;
                consoleSize.X = 40;
                consoleSize.Y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }else{
            //check_and_throw_error("Error after GetConsoleScreenBufferInfo");
            consoleSize.X = 80;
            consoleSize.Y = 30;
        }

        // Create the Pseudo Console of the required size, attached to the PTY-end of the pipes
        hr = CreatePseudoConsole(consoleSize, hPipePTYIn, hPipePTYOut, PSEUDOCONSOLE_INHERIT_CURSOR, &pseudoConsoleHandle);
        if(hr == S_OK){
            // Note: We can close the handles to the PTY-end of the pipes here
            // because the handles are dup'ed into the ConHost and will be released
            // when the ConPTY is destroyed.
            CloseHandle(hPipePTYOut);
            CloseHandle(hPipePTYIn);
            SetLastError(0);
            return std::make_unique<PseudoConsole>(pseudoConsoleHandle, phPipeOut, phPipeIn);

        }else{
            SetLastError(0);
            throw WindowsError("Something went wrong in creating pseudo console error: " + std::to_string(hr));
        }
}
STARTUPINFOEXW Alias::CreateStartupInfoForConsole(PseudoConsole *console) noexcept(false){
		// TODO Catch and throw errors throughout this block
	SetLastError(0);
	check_and_throw_error();
	STARTUPINFOEXW startup_info {};
	size_t attrListSize{};

	startup_info.StartupInfo.cb = sizeof(STARTUPINFOEX);
	startup_info.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	startup_info.StartupInfo.hStdOutput = console->pipe_out;
	startup_info.StartupInfo.hStdError = console->pipe_out;
	startup_info.StartupInfo.hStdInput = console->pipe_in;

	// Get the size of the thread attribute list.
	InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);
	// Clear the error because this functions always returns as error as per
	// https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-initializeprocthreadattributelist#remarks
	SetLastError(0);
	

	// Allocate a thread attribute list of the correct size
	startup_info.lpAttributeList =
		reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(attrListSize));

	check_and_throw_error();
	// Initialize thread attribute list
	if (startup_info.lpAttributeList
		&& InitializeProcThreadAttributeList(startup_info.lpAttributeList, 1, 0, &attrListSize)) {
		check_and_throw_error();
		// Set Pseudo Console attribute
		UpdateProcThreadAttribute(
			startup_info.lpAttributeList,
			0,
			PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
			console->pseudo_console_handle,
			sizeof(HPCON),
			NULL,
			NULL);
			check_and_throw_error();
			
	}
	else {
		check_and_throw_error();
	}
	SetLastError(0);
	return startup_info;
}
Process::ptr Alias::NewProcess(PseudoConsole *console, std::wstring command_line) noexcept(false){
	auto startupInfo = CreateStartupInfoForConsole(console);
	PROCESS_INFORMATION piClient{};
						
	// TODO Catch and throw errors throughout block
	if(CreateProcessW(
		NULL,                           // No module name - use Command Line
		(wchar_t*)command_line.c_str(),           // Command Line
		NULL,                           // Process handle not inheritable
		NULL,                           // Thread handle not inheritable
		FALSE,                          // Inherit handles
		EXTENDED_STARTUPINFO_PRESENT,   // Creation flags
		NULL,                           // Use parent's environment block
		NULL,                           // Use parent's starting directory 
		&(startupInfo.StartupInfo),     // Pointer to STARTUPINFO
		&piClient) == false)             // Pointer to PROCESS_INFORMATION
		{
			check_and_throw_error(std::string{"Couldn't create process "} + (char*)command_line.c_str());
		};


	auto process = std::make_unique<Process>(startupInfo, piClient);
	console->process_attached(process.get());
	return process;
}
CONSOLE_SCREEN_BUFFER_INFO Alias::GetCursorInfo(HANDLE console){
	CONSOLE_SCREEN_BUFFER_INFO cursor{};
	GetConsoleScreenBufferInfo(console, &cursor);
	return cursor;
}
void Alias::PseudoConsole::process_attached(Alias::Process *process) { 
	// When we create the pseudoconsle, it will emit a position request on pipe_out
	auto output = this->read_output();
	if(output.size() == 4 && std::string{"\x1b[6n"} == output){
		auto cursor_pos = this->get_cursor_position_as_vt();
		DWORD bytes_written = 0;
		if(WriteFile(this->pipe_in, cursor_pos.c_str(), cursor_pos.size()*sizeof(char), &bytes_written, nullptr) == false){
			check_and_throw_error("Failed to write cursor pos to console");
		}
	}
}
std::string Alias::PseudoConsole::read_output(){
	constexpr size_t buffer_size = 512;
	char chars[buffer_size];
	DWORD bytes_read = 0;
	if( ReadFile(this->pipe_out, chars, buffer_size*sizeof(char), &bytes_read, nullptr) == false){
		check_and_throw_error("Failed to read from console");
	}
	return std::string{chars, bytes_read/sizeof(char)};
}
std::string Alias::PseudoConsole::get_cursor_position_as_vt(){
	CONSOLE_SCREEN_BUFFER_INFO cursor = GetCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE));
	std::stringstream cursor_pos{};
	cursor_pos << "\x1b[" << cursor.dwCursorPosition.X << ";" << cursor.dwCursorPosition.Y << "R";
	return std::string{cursor_pos.str()};
}
bool Alias::CheckStdOut(std::string message){
	auto std_out = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD bytes_written = 0;
	auto results = (bool)WriteFile(std_out, message.c_str(), message.size()*sizeof(char), &bytes_written, nullptr);
	if(results == false){
		check_and_throw_error("Couldn't write to stdout console. Wrote chars: " + std::to_string(bytes_written));
	}
	return results;
}
bool Alias::SetupConsoleHost() noexcept(false){
	DWORD consoleMode{};
    HANDLE hPrimaryConsole { GetStdHandle(STD_OUTPUT_HANDLE) };
	std::string error_message{};

    if(GetConsoleMode(hPrimaryConsole, &consoleMode) == false){
		error_message += "Couldn't get console mode, going to try to set it anyway.";
    }
    if(SetConsoleMode(hPrimaryConsole, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN) == false){
		error_message += "Couldn't set console mode.";
		check_and_throw_error(error_message);
	}
	return true;
}