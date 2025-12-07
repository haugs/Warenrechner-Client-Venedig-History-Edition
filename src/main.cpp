#define UNICODE
#define _UNICODE

#include <windows.h>
#include <vector>
#include <string>
#include <sstream>

#include "PopulationReader.h"

// Function prototypes
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

            const PopulationReader::PopulationReadResult readResult = PopulationReader::FetchPopulationCounts();
            if (!readResult.success) {
                MessageBoxW(hwnd, readResult.errorMessage.c_str(), L"Error", MB_ICONERROR);
                SetWindowTextW(hEdit, readResult.errorMessage.c_str());
                EnableWindow(hButton, TRUE);
                break;
            }

            struct PopulationMapping {
                std::string token;
                std::wstring displayName;
                std::vector<std::wstring> sourceKeys;
            };

            static const std::vector<PopulationMapping> populationMappings = {
                {"bettler", L"Bettler", {L"BettlerAufDerStrasse", L"BettlerInArmenhaus"}},
                {"bauern", L"Bauern", {L"Bauern"}},
                {"buerger", L"Buerger", {L"Buerger"}},
                {"patrizier", L"Patrizier", {L"Patrizier"}},
                {"adlige", L"Adlige", {L"Adlige"}},
                {"nomaden", L"Nomaden", {L"Nomaden"}},
                {"gesandte", L"Gesandte", {L"Gesandte"}}
            };

            std::wstringstream wss;
            std::ostringstream oss;

            wss << L"Process ID: " << readResult.processId << L"\r\n";
            wss << L"Base Address: 0x" << std::hex << readResult.moduleBase << std::dec << L"\r\n\r\n";
            wss << L"Reading iteration " << iteration << L":\r\n\r\n";

            for (const auto& mapping : populationMappings) {
                uint32_t total = 0;
                for (const auto& key : mapping.sourceKeys) {
                    const auto it = readResult.counts.find(key);
                    if (it != readResult.counts.end()) {
                        total += it->second;
                    }
                }

                wss << mapping.displayName << L": " << total << L"\r\n";
                oss << mapping.token << "." << total << ":";
            }

            std::string combinedString = oss.str();
            if (!combinedString.empty() && combinedString.back() == ':') {
                combinedString.pop_back();
            }

            CopyToClipboard(combinedString, hwnd);
            SetWindowTextW(hEdit, wss.str().c_str());

            ++iteration;
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
