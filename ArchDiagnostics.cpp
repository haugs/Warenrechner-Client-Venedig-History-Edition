#include "ArchDiagnostics.h"

#include <sstream>

namespace {

std::wstring MachineToString(USHORT machine) {
    switch (machine) {
    case IMAGE_FILE_MACHINE_I386: return L"x86";
    case IMAGE_FILE_MACHINE_AMD64: return L"x64";
    case IMAGE_FILE_MACHINE_ARM64: return L"ARM64";
    case IMAGE_FILE_MACHINE_IA64: return L"Itanium";
    case IMAGE_FILE_MACHINE_UNKNOWN: return L"Unknown";
    default: {
        std::wstringstream ss;
        ss << L"0x" << std::hex << machine;
        return ss.str();
    }
    }
}

std::wstring InterpretProcessArchitecture(USHORT processMachine, USHORT nativeMachine) {
    if (processMachine == IMAGE_FILE_MACHINE_UNKNOWN) {
        return L"Native " + MachineToString(nativeMachine) + L" process";
    }

    std::wstring procArch = MachineToString(processMachine);
    std::wstring nativeArch = MachineToString(nativeMachine);
    return procArch + L" process running under WOW on " + nativeArch;
}

} // namespace

std::wstring DescribeProcessArchitecture(HANDLE hProcess) {
    std::wstringstream ss;
    ss << L"Tool architecture: " << (sizeof(void*) == 8 ? L"64-bit" : L"32-bit") << L"\r\n";

    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel) {
        ss << L"GetModuleHandleW(kernel32) failed: " << GetLastError() << L"\r\n";
        return ss.str();
    }

    using IsWow64Process2Fn = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
    using IsWow64ProcessFn = BOOL(WINAPI*)(HANDLE, PBOOL);

    auto pIsWow64Process2 = reinterpret_cast<IsWow64Process2Fn>(GetProcAddress(hKernel, "IsWow64Process2"));
    if (pIsWow64Process2) {
        USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN;
        USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
        if (pIsWow64Process2(hProcess, &processMachine, &nativeMachine)) {
            ss << L"Target architecture: " << InterpretProcessArchitecture(processMachine, nativeMachine) << L"\r\n";
        } else {
            ss << L"IsWow64Process2 failed with error: " << GetLastError() << L"\r\n";
        }
        return ss.str();
    }

    auto pIsWow64Process = reinterpret_cast<IsWow64ProcessFn>(GetProcAddress(hKernel, "IsWow64Process"));
    if (pIsWow64Process) {
        BOOL isWow64 = FALSE;
        if (pIsWow64Process(hProcess, &isWow64)) {
            ss << L"IsWow64Process reports: " << (isWow64 ? L"32-bit process on 64-bit Windows" : L"Native architecture for this OS") << L"\r\n";
        } else {
            ss << L"IsWow64Process failed with error: " << GetLastError() << L"\r\n";
        }
    } else {
        ss << L"IsWow64Process not available on this system." << L"\r\n";
    }

    return ss.str();
}
