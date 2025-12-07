#include "PopulationReader.h"

#include <algorithm>
#include <cwchar>
#include <map>
#include <string>
#include <tlhelp32.h>

#ifndef POPULATION_READER_STANDALONE
#define POPULATION_READER_STANDALONE 1
#endif

namespace PopulationReader {
namespace {

const std::wstring kProcessName = L"Anno1404.exe";

const std::map<std::wstring, int> kPopulationClasses = {
    {L"BettlerAufDerStrasse", -6},
    {L"BettlerInArmenhaus", -5},
    {L"Nomaden", -3},
    {L"Gesandte", -2},
    {L"Bauern", 0},
    {L"Buerger", 1},
    {L"Patrizier", 2},
    {L"Adlige", 3}
};

uint64_t ReadU64(HANDLE processHandle, uint64_t address) {
    uint64_t value = 0;
    ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(address), &value, sizeof(value), nullptr);
    return value;
}

uint32_t ReadU32(HANDLE processHandle, uint64_t address) {
    uint32_t value = 0;
    ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(address), &value, sizeof(value), nullptr);
    return value;
}

uint32_t GetPopulationFromEAD4(HANDLE processHandle, uint64_t moduleBase, int classIndex) {
    uint64_t pointer = ReadU64(processHandle, moduleBase + 0x1F10B90);
    return ReadU32(processHandle, pointer + 0xEAD4 + 4ull * static_cast<uint64_t>(classIndex) * 8ull);
}

std::map<std::wstring, uint32_t> GetPopulationForEachClass(HANDLE processHandle, uint64_t moduleBase) {
    std::map<std::wstring, uint32_t> result;
    for (const auto& entry : kPopulationClasses) {
        result[entry.first] = GetPopulationFromEAD4(processHandle, moduleBase, entry.second);
    }
    return result;
}

DWORD FindProcessIdByName(const std::wstring& exeName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W processEntry = {};
    processEntry.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(snapshot, &processEntry)) {
        do {
            if (exeName == processEntry.szExeFile) {
                pid = processEntry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return pid;
}

uintptr_t GetModuleBaseAddress(DWORD pid, const std::wstring& moduleName) {
    uintptr_t baseAddress = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    MODULEENTRY32W moduleEntry = {};
    moduleEntry.dwSize = sizeof(MODULEENTRY32W);
    if (Module32FirstW(snapshot, &moduleEntry)) {
        std::wstring moduleNameLower = moduleName;
        std::transform(moduleNameLower.begin(), moduleNameLower.end(), moduleNameLower.begin(), ::towlower);

        do {
            std::wstring currentModule = moduleEntry.szModule;
            std::transform(currentModule.begin(), currentModule.end(), currentModule.begin(), ::towlower);
            if (currentModule == moduleNameLower) {
                baseAddress = reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snapshot, &moduleEntry));
    }

    CloseHandle(snapshot);
    return baseAddress;
}

}  // namespace

PopulationReadResult FetchPopulationCounts() {
    PopulationReadResult result;

    DWORD pid = FindProcessIdByName(kProcessName);
    if (pid == 0) {
        result.errorMessage = L"Could not find running process " + kProcessName + L".";
        return result;
    }
    result.processId = pid;

    HANDLE processHandle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!processHandle) {
        result.errorMessage = L"Could not open process " + kProcessName + L". Error code: " + std::to_wstring(GetLastError());
        return result;
    }

    uintptr_t moduleBase = GetModuleBaseAddress(pid, kProcessName);
    if (moduleBase == 0) {
        result.errorMessage = L"Could not find " + kProcessName + L" module.";
        CloseHandle(processHandle);
        return result;
    }
    result.moduleBase = moduleBase;

    result.counts = GetPopulationForEachClass(processHandle, moduleBase);
    result.success = true;

    CloseHandle(processHandle);
    return result;
}

}  // namespace PopulationReader

#if POPULATION_READER_STANDALONE
int wmain() {
    const auto result = PopulationReader::FetchPopulationCounts();
    if (!result.success) {
        fwprintf(stderr, L"Error: %s\n", result.errorMessage.c_str());
        return 1;
    }

    for (const auto& entry : result.counts) {
        wprintf(L"%s population: %u\n", entry.first.c_str(), entry.second);
    }

    return 0;
}
#endif  // POPULATION_READER_STANDALONE