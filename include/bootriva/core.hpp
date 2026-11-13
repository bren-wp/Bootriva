#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
namespace bootriva {
struct ImageInfo {
    std::uint64_t bytes{};
    bool mbr{};
    bool gpt{};
    bool iso9660{};
    bool is_iso_extension{};
    bool hybrid_writable{};
    std::string error;
};
ImageInfo inspect_image(const std::filesystem::path& image);
struct SafetyInput {
    bool usb_bus{};
    bool removable{};
    bool system_disk{};
    bool same_source_disk{};
    bool identity_matched{};
    std::uint64_t drive_bytes{};
    ImageInfo image;
};
struct SafetyResult {
    bool allowed{};
    std::string reason;
};
SafetyResult assess(const SafetyInput& input);
std::string human_size(std::uint64_t bytes);
}
