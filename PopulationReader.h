#pragma once

#include <Windows.h>
#include <cstdint>
#include <map>
#include <string>

namespace PopulationReader {

struct PopulationReadResult {
    bool success = false;
    DWORD processId = 0;
    uintptr_t moduleBase = 0;
    std::map<std::wstring, uint32_t> counts;
    std::wstring errorMessage;
};

PopulationReadResult FetchPopulationCounts();

}  // namespace PopulationReader
