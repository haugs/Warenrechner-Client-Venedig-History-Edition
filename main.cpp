#define UNICODE
#define _UNICODE

#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include "ArchDiagnostics.h"

// Global variables
const std::wstring windowTitle = L"ANNO 1404 Venedig";
const std::wstring processName = L"Anno1404Addon.exe";
const uintptr_t offset = 0x02099B08;

// Function prototypes
DWORD GetProcessID(const std::wstring& windowTitle);
uintptr_t GetModuleBaseAddress(DWORD pid, const std::wstring& moduleName);
void CopyToClipboard(const std::string& text, HWND hwndOwner);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Entry point
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"MemoryReaderWindowClass";

    // Register the window class.
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    // Define the window style without the resize options
    DWORD windowStyle = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;

    // Create the window.
    HWND hwnd = CreateWindowExW(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"Anno 1404 Venedig Warenrechner Client",  // Window text
        windowStyle,                    // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,    // Size and position
        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );


    if (!hwnd) {
        MessageBoxW(NULL, L"Failed to create window.", L"Error", MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // Run the message loop.
    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

// Function implementations
DWORD GetProcessID(const std::wstring& windowTitle) {
    HWND hwnd = FindWindowW(NULL, windowTitle.c_str());
    if (!hwnd) {
        return 0;
    }
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid;
}

uintptr_t GetModuleBaseAddress(DWORD pid, const std::wstring& moduleName) {
    uintptr_t baseAddress = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me;
        me.dwSize = sizeof(MODULEENTRY32W);
        if (Module32FirstW(snapshot, &me)) {
            do {
                // Compare module names case-insensitively
                std::wstring moduleNameLower = moduleName;
                std::wstring szModuleLower = me.szModule;
                std::transform(moduleNameLower.begin(), moduleNameLower.end(), moduleNameLower.begin(), ::towlower);
                std::transform(szModuleLower.begin(), szModuleLower.end(), szModuleLower.begin(), ::towlower);

                if (moduleNameLower == szModuleLower) {
                    baseAddress = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                    break;
                }
            } while (Module32NextW(snapshot, &me));
        }
        CloseHandle(snapshot);
    }
    return baseAddress;
}

void CopyToClipboard(const std::string& text, HWND hwndOwner) {
    // Open the clipboard
    if (!OpenClipboard(hwndOwner)) {
        MessageBoxW(hwndOwner, L"Failed to open clipboard.", L"Error", MB_ICONERROR);
        return;
    }

    // Empty the clipboard
    if (!EmptyClipboard()) {
        MessageBoxW(hwndOwner, L"Failed to empty clipboard.", L"Error", MB_ICONERROR);
        CloseClipboard();
        return;
    }

    // Allocate global memory for the text
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (!hGlob) {
        MessageBoxW(hwndOwner, L"Failed to allocate global memory.", L"Error", MB_ICONERROR);
        CloseClipboard();
        return;
    }

    // Lock the global memory and copy the text
    void* pGlobMem = GlobalLock(hGlob);
    if (!pGlobMem) {
        MessageBoxW(hwndOwner, L"Failed to lock global memory.", L"Error", MB_ICONERROR);
        GlobalFree(hGlob);
        CloseClipboard();
        return;
    }
    memcpy(pGlobMem, text.c_str(), text.size() + 1);
    GlobalUnlock(hGlob);

    // Set the clipboard data
    if (!SetClipboardData(CF_TEXT, hGlob)) {
        MessageBoxW(hwndOwner, L"Failed to set clipboard data.", L"Error", MB_ICONERROR);
        GlobalFree(hGlob);
        CloseClipboard();
        return;
    }

    // Close the clipboard
    CloseClipboard();

    // The clipboard now owns the global memory, so we don't free it
    //MessageBoxW(hwndOwner, L"Text copied to clipboard successfully.", L"Success", MB_ICONINFORMATION);
}


// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hButton;
    static HWND hEdit;
    static int iteration = 1;

    switch (msg) {
    case WM_CREATE: {
        // Create a button
        hButton = CreateWindowW(
            L"BUTTON",  // Predefined class; Unicode assumed
            L"Bewohner auslesen und in die Zwischenablage kopieren",      // Button text
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles
            20,         // x position
            20,         // y position
            440,        // Button width
            30,        // Button height
            hwnd,       // Parent window
            (HMENU)1,   // Control identifier
            (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
            NULL);      // Pointer not needed.

        // Create an edit control
        hEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            20,
            60,
            440,
            280,
            hwnd,
            (HMENU)2,
            GetModuleHandleW(NULL),
            NULL);
    }
                  break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {  // Button clicked
            // Disable the button to prevent multiple clicks
            EnableWindow(hButton, FALSE);

            // Perform the memory reading and copying operation
            std::wstringstream wss;
            std::stringstream ss;

            // Get the Process ID
            DWORD pid = GetProcessID(windowTitle);
            if (pid == 0) {
                MessageBoxW(hwnd, L"Failed to get Process ID.", L"Error", MB_ICONERROR);
                EnableWindow(hButton, TRUE);
                break;
            }
            else {
                wss << L"Process ID: " << pid << L"\r\n";
            }

            // Open the Process
            HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (!hProcess) {
                std::wstring errMsg = L"Failed to open process. Error code: " + std::to_wstring(GetLastError());
                MessageBoxW(hwnd, errMsg.c_str(), L"Error", MB_ICONERROR);
                EnableWindow(hButton, TRUE);
                break;
            }

            // Get the Base Address
            uintptr_t baseAddress = GetModuleBaseAddress(pid, processName);
            if (baseAddress == 0) {
                // Debug: List all modules in the process
                std::wstringstream debugMsg;
                debugMsg << L"Failed to get module base address for: " << processName << L"\r\n\r\n";
                debugMsg << DescribeProcessArchitecture(hProcess) << L"\r\n";
                debugMsg << L"Modules found in process (PID: " << pid << L"):\r\n";
                
                HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
                if (snapshot == INVALID_HANDLE_VALUE) {
                    DWORD err = GetLastError();
                    debugMsg << L"CreateToolhelp32Snapshot failed with error: " << err << L"\r\n";
                    if (err == ERROR_ACCESS_DENIED) {
                        debugMsg << L"(ERROR_ACCESS_DENIED - Try running as Administrator)\r\n";
                    } else if (err == ERROR_PARTIAL_COPY) {
                        debugMsg << L"(ERROR_PARTIAL_COPY - 32/64-bit mismatch between this tool and target process)\r\n";
                    }
                } else {
                    MODULEENTRY32W me;
                    me.dwSize = sizeof(MODULEENTRY32W);
                    if (Module32FirstW(snapshot, &me)) {
                        int count = 0;
                        do {
                            debugMsg << L"  - " << me.szModule << L" (Base: 0x" << std::hex << reinterpret_cast<uintptr_t>(me.modBaseAddr) << std::dec << L")\r\n";
                            count++;
                            if (count >= 20) {
                                debugMsg << L"  ... (showing first 20 modules only)\r\n";
                                break;
                            }
                        } while (Module32NextW(snapshot, &me));
                    } else {
                        debugMsg << L"Module32FirstW failed with error: " << GetLastError() << L"\r\n";
                    }
                    CloseHandle(snapshot);
                }
                
                SetWindowTextW(hEdit, debugMsg.str().c_str());
                CloseHandle(hProcess);
                EnableWindow(hButton, TRUE);
                break;
            }
            else {
                wss << L"Base Address: 0x" << std::hex << baseAddress << std::dec << L"\r\n";
            }

            // Step 1: Calculate the initial Target Address
            uintptr_t initialAddress = baseAddress + offset;

            wss << L"\r\nReading iteration " << iteration << L":\r\n\r\n";

            // Step 2: Read a pointer from the initial Target Address
            uintptr_t intermediateAddress = 0;
            SIZE_T bytesRead = 0;
            if (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(initialAddress), &intermediateAddress, sizeof(intermediateAddress), &bytesRead) && bytesRead == sizeof(intermediateAddress)) {
                //wss << L"Intermediate Address read from 0x" << std::hex << initialAddress << L": 0x" << intermediateAddress << std::dec << L"\r\n";

                // Define the list of additional offsets and names
                struct OffsetInfo {
                    uintptr_t offset;
                    std::string name;
                };
                std::vector<OffsetInfo> offsets = {
                    {0x8E4, "bettler"},
                    {0x984, "bauern"},
                    {0x9A4, "buerger"},
                    {0x9C4, "patrizier"},
                    {0x9E4, "adlige"},
                    {0x924, "nomaden"},
                    {0x944, "gesandte"}
                };

                // String stream to build the combined string
                std::ostringstream oss;

                // Read the values for each offset
                for (const auto& info : offsets) {
                    uintptr_t finalAddress = intermediateAddress + info.offset;
                    uint32_t value = 0;
                    if (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(finalAddress), &value, sizeof(value), &bytesRead) && bytesRead == sizeof(value)) {
                        // Ensure the output is in decimal
                        wss << info.name.c_str() << L": " << std::dec << value << L"\r\n";

                        // Append the value to the string stream in the desired format
                        oss << info.name << "." << value << ":";
                    }
                    else {
                        wss << L"Failed to read memory at address 0x" << std::hex << finalAddress << L" for " << info.name.c_str() << L". Error code: " << GetLastError() << std::dec << L"\r\n";
                    }
                }

                // Remove the trailing colon, if any
                std::string combinedString = oss.str();
                if (!combinedString.empty() && combinedString.back() == ':') {
                    combinedString.pop_back();
                }

                // Display the combined string
                //wss << L"Combined String: " << combinedString.c_str() << L"\r\n";

                // Copy the combined string to the clipboard
                CopyToClipboard(combinedString, hwnd);

                ++iteration;
            }
            else {
                wss << L"Failed to read pointer from initial target address. Error code: " << GetLastError() << L"\r\n";
            }

            // Close the process handle
            CloseHandle(hProcess);

            // Set the text in the edit control
            SetWindowTextW(hEdit, wss.str().c_str());

            // Re-enable the button
            EnableWindow(hButton, TRUE);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}
