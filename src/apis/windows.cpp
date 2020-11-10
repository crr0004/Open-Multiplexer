#include "apis/alias.hpp"
#include <string_view>
/**
 * Welcome to the mess of the Windows API usage for creating and managing PTY and processes.
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
Alias::PseudoConsole::ptr Alias::CreatePseudoConsole(int x, int y, int rows, int columns) noexcept(false){
        SetLastError(0);
        HRESULT hr{ E_UNEXPECTED };
        HANDLE hPipePTYIn{ INVALID_HANDLE_VALUE };
        HANDLE hPipePTYOut{ INVALID_HANDLE_VALUE };
        HANDLE pipe_write_handle{ INVALID_HANDLE_VALUE};
        HANDLE pipe_read_handle{INVALID_HANDLE_VALUE};
        HPCON pseudo_console_handle{};

        // Create the pipes to which the ConPTY will connect
        CreatePipe(&hPipePTYIn, &pipe_write_handle, NULL, Alias::READ_BUFFER_SIZE*sizeof(char));
        CreatePipe(&pipe_read_handle, &hPipePTYOut, NULL, Alias::READ_BUFFER_SIZE*sizeof(char));
        check_and_throw_error();
        // Determine required size of Pseudo Console
        COORD consoleSize{};
        check_and_throw_error();
		consoleSize.X = rows;
		consoleSize.Y = columns;

        // Create the Pseudo Console of the required size, attached to the PTY-end of the pipes
        hr = CreatePseudoConsole(consoleSize, hPipePTYIn, hPipePTYOut, 0, &pseudo_console_handle);
        if(hr == S_OK){
            // Note: We can close the handles to the PTY-end of the pipes here
            // because the handles are dup'ed into the ConHost and will be released
            // when the ConPTY is destroyed.
            CloseHandle(hPipePTYOut);
            CloseHandle(hPipePTYIn);
            SetLastError(0);

            return std::make_unique<PseudoConsole>(x, y, pseudo_console_handle, pipe_write_handle, pipe_read_handle);

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
Alias::Process::ptr Alias::NewProcess(PseudoConsole *console, std::wstring command_line) noexcept(false){
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
	if(GetConsoleScreenBufferInfo(console, &cursor) == false){
		//check_and_throw_error("Couldn't get cursor info");
	}
	return cursor;
}
void Alias::PseudoConsole::process_attached(Alias::Process *process) { 
	// When we create the pseudoconsle, it will emit a position request on pipe_out
	// due to submitting PSEUDOCONSOLE_INHERIT_CURSOR when creating the console
	/*
	auto output = this->latest_output();
	if(output.size() == 4 && std::string{"\x1b[6n"} == output){
		auto cursor_pos = this->get_cursor_position_as_vt(this->x, this->y);
		DWORD bytes_written = 0;
		if(WriteFile(this->pipe_in, cursor_pos.data(), cursor_pos.size()*sizeof(char), &bytes_written, nullptr) == false){
			check_and_throw_error("Failed to write cursor pos to console");
		}
		output_buffer.pop_back();
		// We don't want the cursor position request existing in the buffer
	}
	*/
		this->read_output();
		output_buffer.clear();
		output_buffer.push_back(
			{ "\x1b[" + std::to_string(y) + ";" + std::to_string(x) + "H" }
		);
}
std::vector<std::string>::iterator Alias::split_string(std::string_view string_to_split, char delimeter, std::vector<std::string> *buffer){
	size_t index_of_first_new_string = buffer->size();
	// Don't want to cause an underflow with the -1
	if(buffer->empty()){
		index_of_first_new_string = 0;
	}
	/**
	 * I know this seems bizare, but what it does is split the read buffer based on new lines.
	 * However there are two extra things we need to account for. We want the new line in the string split,
	 * and we want the last section of output even if it doesn't contain a new line.
	 * 
	 * To include the new line we need to +1 to the start position and length.
	 * To include the last section of output, we need to substr with the bytes_read,
	 * which is the rest of the read string.
	 * We also use bytes_read to break the loop.
	 */
	size_t i = 0, start_pos = 0, length = 0;
	do{
		i = string_to_split.find_first_of(delimeter, start_pos);
		length = i+1-start_pos;
		if(i == std::string::npos){
			length = std::string::npos;
		}
		auto string_to_copy = string_to_split.substr(start_pos, length);
		buffer->push_back(std::string{string_to_copy.begin(), string_to_copy.end()});
		start_pos = i+1;
	// Account for two ending conditions. 
	// The string ends with new line so we check the +1 goes to buffer size.
	// The string doesn't end with a new line so we substr the rest and i is npos
	}while(start_pos != string_to_split.size() && i != std::string::npos);
	return buffer->begin()+index_of_first_new_string; // Want to move the iterator to first instance of the new strings
}
Alias::PseudoConsole::BufferIterator Alias::PseudoConsole::read_output(){
	std::string chars(Alias::READ_BUFFER_SIZE, 'a');
	size_t index_of_first_new_string = output_buffer.size()-1;
	if (output_buffer.empty()) {
		index_of_first_new_string = 0;
	}
	DWORD bytes_read = 0;
	do{
		if( ReadFile(this->pipe_out, chars.data(), Alias::READ_BUFFER_SIZE*sizeof(char), &bytes_read, nullptr) == false){
			check_and_throw_error("Failed to read from console");
		}
		std::string_view string_to_split{chars.data(), bytes_read*sizeof(char)};
		Alias::split_string(string_to_split, (char)'\n', &output_buffer);

	}while(this->bytes_in_read_pipe() > 0);

	this->last_read_in = chars.substr(0, bytes_read*sizeof(char)); // Keep a copy of the last read so we can test with it.
	return output_buffer.begin()+index_of_first_new_string; // Want to move the iterator to first instance of the new strings
}
size_t Alias::PseudoConsole::write_to_stdout(std::string &input){
	return this->write(input, GetStdHandle(STD_OUTPUT_HANDLE));
}
size_t Alias::PseudoConsole::write(std::string &input, HANDLE handle){
	DWORD bytes_written = 0;	
	if(WriteFile(handle, input.data(), input.size()*sizeof(char), &bytes_written, nullptr) == false){
		check_and_throw_error("Failed to write to handle");
	}
	return bytes_written;
}
size_t Alias::PseudoConsole::bytes_in_read_pipe(){
	DWORD bytes_in_pipe = 0;
	if(PeekNamedPipe(
		this->pipe_out,
		nullptr, // don't want to actually copy the data
		0,
		nullptr,
		&bytes_in_pipe,
		nullptr	
	) == false){
		check_and_throw_error("Couldn't peek at read_pipe");
	}
	return bytes_in_pipe;
}
std::string Alias::PseudoConsole::get_cursor_position_as_vt(int x, int y){
	std::stringstream cursor_pos{};
	cursor_pos << "\x1b[" << x << ";" << y << "R";
	return std::string{cursor_pos.str()};
}
std::string Alias::PseudoConsole::get_cursor_position_as_movement(){
	HANDLE stdout_handle{GetStdHandle(STD_OUTPUT_HANDLE)};
	auto cursor_info = Alias::GetCursorInfo(stdout_handle);
	std::stringstream cursor_pos{};
	cursor_pos << "\x1b[" 
		<< cursor_info.dwCursorPosition.Y << ";"
		<< cursor_info.dwCursorPosition.X << "H";
	return std::string{cursor_pos.str()};

}
void Alias::PseudoConsole::write_input(std::string_view input){
	DWORD bytes_written = 0;
	if(WriteFile(this->pipe_in, input.data(), input.size()*sizeof(char), &bytes_written, nullptr) == false){
		check_and_throw_error("Couldn't write to pipe_in");
	}
}


bool Alias::Process::stopped(){
	DWORD exit_code = 0;
	GetExitCodeProcess(this->process_info.hProcess, &exit_code);
	return exit_code != STILL_ACTIVE;
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
    HANDLE hPrimaryConsole = GetStdHandle(STD_OUTPUT_HANDLE) ;
	std::string error_message{};

    if(GetConsoleMode(hPrimaryConsole, &consoleMode) == false){
		error_message += "Couldn't get console mode, going to try to set it anyway.";
    }
    if(SetConsoleMode(hPrimaryConsole, consoleMode 
	| ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_WRAP_AT_EOL_OUTPUT | DISABLE_NEWLINE_AUTO_RETURN
	) == false){
		error_message += "Couldn't set console mode.";
		check_and_throw_error(error_message);
	}
	return true;
}