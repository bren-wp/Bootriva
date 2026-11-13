#include "bootriva/bridge.h"
#include "bootriva/core.hpp"
#include "usb_win.hpp"
#include <atomic>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <new>
#include <string>
#include <vector>
#include <cwchar>
struct BrTask {
    std::atomic_bool cancel{false};
    std::atomic_bool running{false};
};
namespace {
void output_message(wchar_t* destination, uint32_t capacity, const std::wstring& message) noexcept {
    if (!destination || capacity == 0) return;
    const auto count=std::min<std::size_t>(message.size(),capacity-1);
    std::wmemcpy(destination,message.c_str(),count);
    destination[count]=L'\0';
}
template<std::size_t N>
void field(wchar_t (&destination)[N], const std::wstring& text) noexcept {
    const auto count=std::min<std::size_t>(text.size(),N-1);
    std::wmemcpy(destination,text.data(),count);
    destination[count]=L'\0';
}
std::wstring as_wide(const std::string& text){
    // Error strings from bootriva_core are ASCII-based, not locale-specific.
    return std::wstring(text.begin(),text.end());
}
struct InUse {
    BrTask* task;
    bool acquired;
    explicit InUse(BrTask* t):task(t),acquired(false) {
        if(!task)return;
        bool expected=false;
        acquired=task->running.compare_exchange_strong(expected,true);
        // New tasks start uncancelled. Never reset a cancellation requested
        // while the UI worker is being scheduled.
    }
    ~InUse(){if(acquired)task->running.store(false);}
};
bool identical(const BrDrive& selected, const bootriva::win::Drive& current){
    return selected.disk_number==current.disk_number &&
           selected.capacity==current.capacity &&
           selected.removable==(current.removable?1u:0u) &&
           selected.usb_bus==(current.usb_bus?1u:0u) &&
           std::wstring(selected.model)==current.model &&
           std::wstring(selected.serial)==current.serial;
}
}
extern "C" {
uint32_t __cdecl br_abi_version(void){return 0x00020006u;}
uint32_t __cdecl br_list_usb(BrDrive* destination,uint32_t capacity){
    try {
        const auto all=bootriva::win::detect_removable_usb();
        if(destination && capacity){
            const auto count=std::min<std::size_t>(all.size(),capacity);
            for(std::size_t i=0;i<count;i++){
                BrDrive copy{};
                copy.disk_number=all[i].disk_number;
                copy.capacity=all[i].capacity;
                copy.removable=all[i].removable?1u:0u;
                copy.usb_bus=all[i].usb_bus?1u:0u;
                field(copy.model,all[i].model);
                field(copy.serial,all[i].serial);
                destination[i]=copy;
            }
        }
        return static_cast<uint32_t>(all.size());
    }catch(...){return 0;}
}
int32_t __cdecl br_inspect_image(const wchar_t* path,BrImage* output){
    if(!output)return 0;
    *output=BrImage{};
    if(!path || !*path){
        field(output->error,L"Choose an ISO/IMG/BIN file first.");
        return 0;
    }
    try{
        const auto info=bootriva::inspect_image(std::filesystem::path(path));
        output->bytes=info.bytes;
        output->mbr=info.mbr?1u:0u;
        output->gpt=info.gpt?1u:0u;
        output->iso9660=info.iso9660?1u:0u;
        output->is_iso_extension=info.is_iso_extension?1u:0u;
        output->hybrid_writable=info.hybrid_writable?1u:0u;
        field(output->error,as_wide(info.error));
        return info.error.empty()?1:0;
    }catch(...){
        field(output->error,L"Native image inspection failed.");
        return 0;
    }
}
BrTask* __cdecl br_task_create(void){return new(std::nothrow) BrTask;}
void __cdecl br_task_cancel(BrTask* task){if(task)task->cancel.store(true);}
void __cdecl br_task_destroy(BrTask* task){
    // Caller must wait for the blocking operation to return before releasing
    // the task. Avoid freeing a task that is still used by a worker thread.
    if(task && !task->running.load())delete task;
}
int32_t __cdecl br_run_sha256(BrTask* task,const wchar_t* image_path,
                              BrProgress progress,void* context,wchar_t* result,uint32_t chars){
    output_message(result,chars,L"SHA-256 did not start.");
    InUse inUse(task);
    if(!inUse.acquired){output_message(result,chars,L"Operation is already running.");return 0;}
    if(!image_path || !*image_path){output_message(result,chars,L"Missing image path.");return 0;}
    try {
        auto report=[&](unsigned percent,const std::wstring& label){
            if(progress)progress(percent,label.c_str(),context);
        };
        const auto done=bootriva::win::sha256(image_path,task->cancel,report);
        output_message(result,chars,done.message);
        return done.success?1:0;
    }catch(...){
        output_message(result,chars,L"Unexpected native SHA-256 failure.");
        return 0;
    }
}
int32_t __cdecl br_run_flash(BrTask* task,const BrDrive* selected,const wchar_t* image_path,
                             const wchar_t* typed_confirmation,BrProgress progress,void* context,
                             wchar_t* result,uint32_t chars){
    output_message(result,chars,L"Write did not start.");
    InUse inUse(task);
    if(!inUse.acquired){output_message(result,chars,L"Operation is already running.");return 0;}
    if(!selected || !image_path || !*image_path || !typed_confirmation ||
       std::wcscmp(typed_confirmation,L"BRISI")!=0){
        output_message(result,chars,L"Explicit BRISI confirmation and a selected image/disk are required.");
        return 0;
    }
    try {
        // Re-enumerate *physical USB disk identities* just before writing.
        // Never trust a C# drive-letter description or stale disk number.
        const auto all=bootriva::win::detect_removable_usb();
        const auto it=std::find_if(all.begin(),all.end(),
            [&](const bootriva::win::Drive& drive){return identical(*selected,drive);});
        if(it==all.end()){
            output_message(result,chars,L"USB identity changed or device is not an eligible removable USB disk.");
            return 0;
        }
        const auto inspected=bootriva::inspect_image(std::filesystem::path(image_path));
        bool unknown=false;
        const bool sourceOnTarget=bootriva::win::source_on_disk(image_path,it->disk_number,unknown);
        bootriva::SafetyInput safety{it->usb_bus,it->removable,false,sourceOnTarget,
                                     !unknown,it->capacity,inspected};
        const auto decision=bootriva::assess(safety);
        if(!decision.allowed){
            output_message(result,chars,as_wide(decision.reason));
            return 0;
        }
        auto report=[&](unsigned percent,const std::wstring& label){
            if(progress)progress(percent,label.c_str(),context);
        };
        // flash_and_verify performs another device/volume lock/identity
        // validation, then checks *every byte* by reading the drive back.
        const auto completed=bootriva::win::flash_and_verify(*it,image_path,inspected,
                                                              task->cancel,report);
        output_message(result,chars,completed.message);
        return completed.success?1:0;
    }catch(...){
        output_message(result,chars,L"Unexpected native USB write failure.");
        return 0;
    }
}
}
