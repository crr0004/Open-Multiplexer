// EchoCon.cpp : Entry point for the EchoCon Pseudo-Console sample application.
// Copyright © 2018, Microsoft

#include <SDKDDKVer.h>
#include <cstdio>
#include <tchar.h>
#include <Windows.h>
#include <process.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

typedef struct Pipes{
    HANDLE in;
    HANDLE out;
} Pipes;
// Forward declarations
HRESULT CreatePseudoConsoleAndPipes(HPCON*, HANDLE*, HANDLE*);
HRESULT InitializeStartupInfoAttachedToPseudoConsole(STARTUPINFOEXW*, HPCON);
void __cdecl PipeListener(void*);

std::ofstream error_log ;

int main()
{

    error_log.open("error.log", std::ios::out | std::ios::ate);
    auto errorLogIsOpen = error_log.is_open();
    if(!errorLogIsOpen){
        std::cout << "Failed to open error.log\n";
        return -1;
    }else{
        error_log << "opened\n";
        error_log.flush();
    }

    std::wstring szCommand{L"F:\\dev\\bin\\pswh\\pwsh.exe -Login"};
    HRESULT hr{ E_UNEXPECTED };
    HANDLE hPrimaryConsole = GetStdHandle(STD_OUTPUT_HANDLE) ;

    // Enable Console VT Processing
    DWORD consoleMode{};
    if(GetConsoleMode(hPrimaryConsole, &consoleMode) == false){
        return GetLastError();
    }
    hr = SetConsoleMode(hPrimaryConsole, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        ? S_OK
        : GetLastError();
    if (S_OK == hr)
    {
        HPCON hPseudoConsoleHandle{ INVALID_HANDLE_VALUE };

        //  Create the Pseudo Console and pipes to it

        // Pipe for reading output from psuedoconsole
        HANDLE hPipeIn{ INVALID_HANDLE_VALUE };

        // Pipe for writing input to psuedoconsole
        HANDLE hPipeOut{ INVALID_HANDLE_VALUE };
        hr = CreatePseudoConsoleAndPipes(&hPseudoConsoleHandle, &hPipeIn, &hPipeOut);
        Pipes args{hPipeIn, hPipeOut};
        if (S_OK == hr)
        {
            // Create & start thread to listen to the incoming pipe
            // Note: Using CRT-safe _beginthread() rather than CreateThread()
            HANDLE hPipeListenerThread{ reinterpret_cast<HANDLE>(_beginthread(PipeListener, 0, &args)) };

            // Initialize the necessary startup info struct        
            STARTUPINFOEXW startupInfo{};
            if (S_OK == InitializeStartupInfoAttachedToPseudoConsole(&startupInfo, hPseudoConsoleHandle))
            {
                // Launch ping to emit some text back via the pipe
                PROCESS_INFORMATION piClient{};
								
                hr = CreateProcessW(
                    NULL,                           // No module name - use Command Line
                    (wchar_t*)szCommand.data(),                      // Command Line
                    NULL,                           // Process handle not inheritable
                    NULL,                           // Thread handle not inheritable
                    FALSE,                          // Inherit handles
                    EXTENDED_STARTUPINFO_PRESENT,   // Creation flags
                    NULL,                           // Use parent's environment block
                    NULL,                           // Use parent's starting directory 
                    &(startupInfo.StartupInfo),       // Pointer to STARTUPINFO
                    &piClient)                      // Pointer to PROCESS_INFORMATION
                    ? S_OK
                    : GetLastError();

                if (S_OK == hr)
                {
                    // Wait up to 10s for ping process to complete
                   // Allow listening thread to catch-up with final output!
                    Sleep(50000);
                }else{
                    error_log << "Error: " << hr << "\n";
                }

                // --- CLOSEDOWN ---

                // Now safe to clean-up client app's process-info & thread
                CloseHandle(piClient.hThread);
                CloseHandle(piClient.hProcess);

                // Cleanup attribute list
                DeleteProcThreadAttributeList(startupInfo.lpAttributeList);
                free(startupInfo.lpAttributeList);
            }

            // Close ConPTY - this will terminate client process if running
            ClosePseudoConsole(hPseudoConsoleHandle);

            // Clean-up the pipes
            if (INVALID_HANDLE_VALUE != hPipeOut) CloseHandle(hPipeOut);
            if (INVALID_HANDLE_VALUE != hPipeIn) CloseHandle(hPipeIn);
        }
    }

    error_log.close();
    return S_OK == hr ? EXIT_SUCCESS : EXIT_FAILURE;
}

HRESULT CreatePseudoConsoleAndPipes(HPCON* pseudoConsoleHandle, HANDLE* phPipeIn, HANDLE* phPipeOut)
{
    HRESULT hr{ E_UNEXPECTED };
    HANDLE hPipePTYIn{ INVALID_HANDLE_VALUE };
    HANDLE hPipePTYOut{ INVALID_HANDLE_VALUE };

    // Create the pipes to which the ConPTY will connect
    if (CreatePipe(&hPipePTYIn, phPipeOut, NULL, 1024) &&
        CreatePipe(phPipeIn, &hPipePTYOut, NULL, 1024))
    {
        // Determine required size of Pseudo Console
        COORD consoleSize{};

            consoleSize.X = 80;
            consoleSize.Y = 9;
        

        // Create the Pseudo Console of the required size, attached to the PTY-end of the pipes
        hr = CreatePseudoConsole(consoleSize, hPipePTYIn, hPipePTYOut, 0, pseudoConsoleHandle);

        // Note: We can close the handles to the PTY-end of the pipes here
        // because the handles are dup'ed into the ConHost and will be released
        // when the ConPTY is destroyed.
        if (INVALID_HANDLE_VALUE != hPipePTYOut) {
            CloseHandle(hPipePTYOut);
        }
        if (INVALID_HANDLE_VALUE != hPipePTYIn) CloseHandle(hPipePTYIn);
    }

    return hr;
}

// Initializes the specified startup info struct with the required properties and
// updates its thread attribute list with the specified ConPTY handle
HRESULT InitializeStartupInfoAttachedToPseudoConsole(STARTUPINFOEXW* pStartupInfo, HPCON hPC)
{
    HRESULT hr{ E_UNEXPECTED };

    if (pStartupInfo)
    {
        SIZE_T attrListSize{0};

        pStartupInfo->StartupInfo.cb = sizeof(STARTUPINFOEX);

        // Get the size of the thread attribute list.
        InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);

        // Allocate a thread attribute list of the correct size
        pStartupInfo->lpAttributeList =
            reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(attrListSize));

        // Initialize thread attribute list
        if (pStartupInfo->lpAttributeList
            && InitializeProcThreadAttributeList(pStartupInfo->lpAttributeList, 1, 0, &attrListSize))
        {
            // Set Pseudo Console attribute
            hr = UpdateProcThreadAttribute(
                pStartupInfo->lpAttributeList,
                0,
                PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                hPC,
                sizeof(HPCON),
                NULL,
                NULL)
                ? S_OK
                : HRESULT_FROM_WIN32(GetLastError());
        }
        else
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }
    return hr;
}

void DrawVerticaleLine(HANDLE hConsole)
{
    //\x1b(Bx\x1b[1Bx\x1b[1Bx
    std::string renderString{ "\x1b[s\
\x1b[31m\
\x1b(0\
\x1b[10;51H\
x\
\x1b[9;51H\
x\
\x1b(B\
\x1b[m\
\x1b[u" };
    WriteFile(hConsole, renderString.data(), renderString.size() * sizeof(char), nullptr, nullptr);
}

void __cdecl PipeListener(
    //LPVOID hPtyOut, LPVOID hPtyIn
    void *args
) {
    Pipes *pipes = reinterpret_cast<Pipes*>(args);
    HANDLE hPipe{ pipes->in };
    HANDLE hPipeIn{ pipes->out };

    HANDLE hConsole{ GetStdHandle(STD_OUTPUT_HANDLE) };

    const DWORD BUFF_SIZE{ 512 };
    char szBuffer[BUFF_SIZE]{};

    DWORD dwBytesWritten{};
    DWORD dwBytesRead{};
    BOOL fRead{ FALSE };

    auto hLog = CreateFile("pty_1.log", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) ;
    auto error = GetLastError();
    if(error != 0 && error != ERROR_ALREADY_EXISTS){
        error_log << "Something went wrong opening the log file for writing: " << error << "\n";
    }
    SetLastError(0);
   
    do
    {
        WriteFile(hPipeIn, "\r", 1, nullptr, nullptr);
        Sleep(100);
        // Read from the pipe
        fRead = ReadFile(hPipe, szBuffer, BUFF_SIZE, &dwBytesRead, NULL);

        // Write received text to the Console
        // Note: Write to the Console using WriteFile(hConsole...), not printf()/puts() to
        // prevent partially-read VT sequences from corrupting output
        
         WriteFile(hLog, szBuffer, dwBytesRead, &dwBytesWritten, NULL);
         if(std::string{"\x1b[6n"}.compare(0, 4, szBuffer, 4) == 0){
            error_log.write(szBuffer, dwBytesRead);
            CONSOLE_SCREEN_BUFFER_INFO cursor{};
            GetConsoleScreenBufferInfo(hConsole, &cursor);
            std::stringstream cursorPos{};
            cursorPos << "\x1b[" << cursor.dwCursorPosition.X << ";" << cursor.dwCursorPosition.Y << "R";

            error_log.write(cursorPos.str().c_str(), cursorPos.tellp());
            DWORD bytesWritten{};
            WriteFile(hPipeIn, cursorPos.str().c_str(), cursorPos.tellp(), &bytesWritten, nullptr);
            auto error = GetLastError();
            error_log << "\nBytes written: " << bytesWritten << " Error: " << error << std::endl;

         }else{
            WriteFile(hConsole, szBuffer, dwBytesRead, &dwBytesWritten, NULL);
         }
        error_log.flush();
    } while (fRead && dwBytesRead >= 0);
}

