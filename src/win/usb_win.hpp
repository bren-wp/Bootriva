#pragma once
#include "bootriva/core.hpp"
#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace bootriva::win {
struct Drive {
    DWORD disk_number{};
    std::uint64_t capacity{};
    std::wstring model;
    std::wstring serial;
    bool removable{};
    bool usb_bus{};
};
std::vector<Drive> detect_removable_usb();
std::wstring describe(const Drive& drive);
// Contains no elevated-operation side effects; used to preflight USB selection.
bool source_on_disk(const std::wstring& image_path, DWORD target, bool& unknown);
using Progress = std::function<void(unsigned, const std::wstring&)>;
struct Result { bool success{}; std::wstring message; };
Result flash_and_verify(const Drive& selected, const std::wstring& image_path,
                        const ImageInfo& info, const std::atomic_bool& cancel,
                        const Progress& progress);
Result sha256(const std::wstring& image_path, const std::atomic_bool& cancel,
              const Progress& progress);
}
