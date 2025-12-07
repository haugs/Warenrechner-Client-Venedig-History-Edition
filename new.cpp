#include <windows.h>
#include <cstdint>
#include <string>
#include <algorithm>
#include <cwchar>
#include <tlhelp32.h>
#include <map>

const std::wstring processName = L"Anno1404.exe";

std::map<std::wstring, int> populationClasses = {
    {L"BettlerAufDerStrasse", -6},
    {L"BettlerInArmenhaus", -5},    
    {L"Nomaden", -3},
    {L"Gesandte", -2},
    {L"Bauern", 0},
    {L"Buerger", 1},
    {L"Patrizier", 2},
    {L"Adlige", 3}
};

uint64_t ReadU64(HANDLE h, uint64_t addr)
{
    uint64_t v;
    ReadProcessMemory(h, (void*)addr, &v, sizeof(v), nullptr);
    return v;
}

uint32_t ReadU32(HANDLE h, uint64_t addr)
{
    uint32_t v;
    ReadProcessMemory(h, (void*)addr, &v, sizeof(v), nullptr);
    return v;
}

// Functions to get population data
uint32_t GetPop_EAD4(HANDLE hProc, uint64_t moduleBase, int idxClass)
{
    uint64_t P = ReadU64(hProc, moduleBase + 0x1F10B90);
    return ReadU32(hProc, P + 0xEAD4 + 4ull*idxClass*8ull);
}


uint32_t GetPop_ByClassName(HANDLE hProc, uint64_t moduleBase, const std::wstring& className)
{
    auto it = populationClasses.find(className);
    if (it != populationClasses.end())
    {
        return GetPop_EAD4(hProc, moduleBase, it->second);
    }
    return 0; // or some error value
}

std::map<std::wstring, uint32_t> GetPop_EachClassName(HANDLE hProc, uint64_t moduleBase)
{
    std::map<std::wstring, uint32_t> result;
    for (const auto& pair : populationClasses)
    {
        result[pair.first] = GetPop_EAD4(hProc, moduleBase, pair.second);
    }
    return result;
}

// I cant remember why I made 3 functions that do the same thing with different offsets
uint32_t GetPop_E9D4(HANDLE hProc, uint64_t moduleBase, int idxClass)
{
    uint64_t P = ReadU64(hProc, moduleBase + 0x1F10B90);
    return ReadU32(hProc, P + 0xE9D4 + 4ull*idxClass);
}

// I cant remember why I made 3 functions that do the same thing with different offsets
uint32_t GetPop_E9F4(HANDLE hProc, uint64_t moduleBase, int idxClass, int idxSomething)
{
    uint64_t P = ReadU64(hProc, moduleBase + 0x1F10B90);
    int index = idxClass + 8 * idxSomething;
    return ReadU32(hProc, P + 0xE9F4 + 4ull*index);
}

DWORD FindProcessIdByName(const std::wstring& exeName)
{
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(snapshot, &pe))
    {
        do
        {
            if (exeName == pe.szExeFile)
            {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &pe));
    }

    CloseHandle(snapshot);
    return pid;
}

//try the GetPop functions with a handful of idxes
int main()
{
    DWORD pid = FindProcessIdByName(processName);
    if (pid == 0)
    {
        std::wstring msg = L"Could not find running process " + processName + L".";
        fwprintf(stderr, L"Error: %s\n", msg.c_str());
        return 1;
    }

    HANDLE hProc = OpenProcess(PROCESS_VM_READ, FALSE, pid);
    if (!hProc)
    {
        std::wstring msg = L"Could not open process " + processName + L".";
        fwprintf(stderr, L"Error: %s\n", msg.c_str());
        return 1;
    }

    uintptr_t moduleBase = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        MODULEENTRY32W me;
        me.dwSize = sizeof(MODULEENTRY32W);
        if (Module32FirstW(snapshot, &me))
        {
            do
            {
                std::wstring modName = me.szModule;
                if (modName == processName)
                {
                    moduleBase = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                    break;
                }
            } while (Module32NextW(snapshot, &me));
        }
        CloseHandle(snapshot);
    }

    if (moduleBase == 0)
    {
        std::wstring msg = L"Could not find " + processName + L" module.";
        fwprintf(stderr, L"Error: %s\n", msg.c_str());
        CloseHandle(hProc);
        return 1;
    }

    auto populationCounts = GetPop_EachClassName(hProc, moduleBase);
    for (const auto& pair : populationCounts)
    {
        wprintf(L"%s population: %u\n", pair.first.c_str(), pair.second);
    }

    CloseHandle(hProc);
    return 0;
}