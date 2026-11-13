#pragma once
// Stable, versioned Win64 C ABI for native WinUI/.NET interop.
// No C++ strings, exceptions, allocations or STL types cross this boundary.
#if !defined(_WIN32)
#error "BOOTRIVA bridge supports Windows only."
#endif
#include <stdint.h>
#include <wchar.h>
#if defined(BOOTRIVA_BRIDGE_EXPORTS)
#define BR_API __declspec(dllexport)
#else
#define BR_API __declspec(dllimport)
#endif
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct BrDrive {
    uint32_t disk_number;
    uint64_t capacity;
    uint32_t removable;
    uint32_t usb_bus;
    wchar_t model[128];
    wchar_t serial[128];
} BrDrive;
typedef struct BrImage {
    uint64_t bytes;
    uint32_t mbr;
    uint32_t gpt;
    uint32_t iso9660;
    uint32_t is_iso_extension;
    uint32_t hybrid_writable;
    wchar_t error[256];
} BrImage;
typedef struct BrTask BrTask;
// The callback may execute on a worker thread. Never invoke WinUI controls
// directly from it; enqueue work through DispatcherQueue instead.
typedef void (__cdecl *BrProgress)(uint32_t percent, const wchar_t* message, void* context);
BR_API uint32_t __cdecl br_abi_version(void);
BR_API uint32_t __cdecl br_list_usb(BrDrive* destination, uint32_t capacity);
BR_API int32_t __cdecl br_inspect_image(const wchar_t* image_path, BrImage* output);
BR_API BrTask* __cdecl br_task_create(void);
BR_API void __cdecl br_task_cancel(BrTask* task);
// Only call destroy after the associated br_run_* call returns.
BR_API void __cdecl br_task_destroy(BrTask* task);
// Outputs are caller-owned UTF-16 buffers; every failed operation includes
// a readable reason. Returns 1 on success and 0 on failure.
BR_API int32_t __cdecl br_run_sha256(
    BrTask* task, const wchar_t* image_path,
    BrProgress progress, void* context, wchar_t* result, uint32_t chars);
BR_API int32_t __cdecl br_run_flash(
    BrTask* task, const BrDrive* selected, const wchar_t* image_path,
    const wchar_t* typed_confirmation, BrProgress progress, void* context,
    wchar_t* result, uint32_t chars);
#if defined(__cplusplus)
}
#endif
