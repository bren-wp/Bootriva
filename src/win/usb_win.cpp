#include "usb_win.hpp"
#include <bcrypt.h>
#include <winioctl.h>
#include <algorithm>
#include <cstring>
#include <array>
#include <memory>
#include <sstream>
#include <vector>
#pragma comment(lib, "bcrypt.lib")
namespace bootriva::win {
namespace {
class Handle {
public:
    explicit Handle(HANDLE h=INVALID_HANDLE_VALUE):h_(h){}
    ~Handle(){if(valid()) CloseHandle(h_);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
    Handle(Handle&& o) noexcept:h_(o.h_){o.h_=INVALID_HANDLE_VALUE;}
    Handle& operator=(Handle&& o) noexcept {if(this!=&o){if(valid())CloseHandle(h_);h_=o.h_;o.h_=INVALID_HANDLE_VALUE;}return *this;}
    bool valid()const{return h_!=INVALID_HANDLE_VALUE && h_!=nullptr;}
    HANDLE get()const{return h_;}
private:HANDLE h_;
};
std::wstring error_text(DWORD code=GetLastError()) {
    LPWSTR msg=nullptr;
    DWORD n=FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr,code,0,reinterpret_cast<LPWSTR>(&msg),0,nullptr);
    std::wstring s=n && msg ? std::wstring(msg,n) : L"Unknown Windows error";
    if(msg)LocalFree(msg);
    while(!s.empty() && (s.back()==L'\r'||s.back()==L'\n'))s.pop_back();
    return s;
}
std::wstring physical(DWORD num) {return L"\\\\.\\PhysicalDrive"+std::to_wstring(num);}
std::wstring ansi_text(const char* value) {
    if(!value) return {};
    int n=MultiByteToWideChar(CP_ACP,0,value,-1,nullptr,0);
    if(n<=1)return {};
    std::wstring out(static_cast<std::size_t>(n),L'\0');
    if(MultiByteToWideChar(CP_ACP,0,value,-1,out.data(),n)<=0)return {};
    out.resize(static_cast<std::size_t>(n-1));
    return out;
}
bool query_disk(DWORD num,Drive& out){
    Handle h(CreateFileW(physical(num).c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
    if(!h.valid())return false;
    STORAGE_PROPERTY_QUERY query{};query.PropertyId=StorageDeviceProperty;query.QueryType=PropertyStandardQuery;
    alignas(STORAGE_DEVICE_DESCRIPTOR) std::array<BYTE,4096> b{};DWORD got=0;
    if(!DeviceIoControl(h.get(),IOCTL_STORAGE_QUERY_PROPERTY,&query,sizeof(query),b.data(),static_cast<DWORD>(b.size()),&got,nullptr)
       ||got<sizeof(STORAGE_DEVICE_DESCRIPTOR))return false;
    const auto* d=reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(b.data());
    if(d->Size>got)return false;
    GET_LENGTH_INFORMATION len{};
    if(!DeviceIoControl(h.get(),IOCTL_DISK_GET_LENGTH_INFO,nullptr,0,&len,sizeof(len),&got,nullptr)||len.Length.QuadPart<=0)return false;
    auto str=[&](DWORD off)->std::wstring {
        if(off==0 || off>=got || !std::memchr(b.data()+off,0,got-off))return {};
        return ansi_text(reinterpret_cast<const char*>(b.data()+off));
    };
    out.disk_number=num;out.capacity=static_cast<std::uint64_t>(len.Length.QuadPart);
    out.usb_bus=(d->BusType==BusTypeUsb);out.removable=d->RemovableMedia!=0;
    out.model=str(d->VendorIdOffset)+L" "+str(d->ProductIdOffset);
    out.serial=str(d->SerialNumberOffset);
    if(out.model==L" ")out.model=L"USB storage device";
    return true;
}
// Match volume extents, including volumes with more than one disk extent.
bool volume_contains_disk(HANDLE volume,DWORD disk,bool& contains){
    contains=false;
    alignas(VOLUME_DISK_EXTENTS) std::array<BYTE,8192> buffer{};DWORD out=0;
    if(!DeviceIoControl(volume,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,nullptr,0,buffer.data(),static_cast<DWORD>(buffer.size()),&out,nullptr))return false;
    auto* ext=reinterpret_cast<const VOLUME_DISK_EXTENTS*>(buffer.data());
    if(out<sizeof(VOLUME_DISK_EXTENTS) || ext->NumberOfDiskExtents>128)return false;
    for(DWORD i=0;i<ext->NumberOfDiskExtents;i++)if(ext->Extents[i].DiskNumber==disk)contains=true;
    return true;
}
std::wstring volume_device(const std::wstring& guid){
    if(!guid.empty() && guid.back()==L'\\') return guid.substr(0,guid.size()-1);
    return guid;
}
bool disk_for_path(const std::wstring& p,DWORD target,bool& contains){
    contains=false;
    wchar_t root[MAX_PATH]{};
    if(!GetVolumePathNameW(p.c_str(),root,MAX_PATH))return false;
    wchar_t volume[MAX_PATH]{};
    if(!GetVolumeNameForVolumeMountPointW(root,volume,MAX_PATH))return false;
    Handle h(CreateFileW(volume_device(volume).c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,
                         nullptr,OPEN_EXISTING,0,nullptr));
    return h.valid() && volume_contains_disk(h.get(),target,contains);
}
bool safe_system_check(DWORD disk){
    wchar_t path[MAX_PATH]{};
    if(!GetWindowsDirectoryW(path,MAX_PATH))return false;
    bool same=false;
    return disk_for_path(path,disk,same) && !same;
}
bool lock_volumes(DWORD target,std::vector<Handle>& out,std::wstring& error){
    wchar_t volume[MAX_PATH]{};
    HANDLE it=FindFirstVolumeW(volume,MAX_PATH);
    if(it==INVALID_HANDLE_VALUE){error=L"Cannot enumerate volumes: "+error_text();return false;}
    bool success=true;
    do {
        std::wstring device=volume_device(volume);
        Handle read(CreateFileW(device.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
        bool contains=false;
        if(read.valid() && volume_contains_disk(read.get(),target,contains) && contains){
            // The query-only handle is discarded before requesting the exclusive filesystem lock.
            read=Handle();
            Handle exclusive(CreateFileW(device.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                                         nullptr,OPEN_EXISTING,0,nullptr));
            DWORD returned=0;
            if(!exclusive.valid() || !DeviceIoControl(exclusive.get(),FSCTL_LOCK_VOLUME,nullptr,0,nullptr,0,&returned,nullptr)){
                error=L"Cannot lock all USB volumes. Close programs using the USB drive. "+error_text();success=false;break;
            }
            if(!DeviceIoControl(exclusive.get(),FSCTL_DISMOUNT_VOLUME,nullptr,0,nullptr,0,&returned,nullptr)){
                error=L"Cannot dismount USB volume: "+error_text();success=false;break;
            }
            out.push_back(std::move(exclusive));
        }
    }while(FindNextVolumeW(it,volume,MAX_PATH));
    DWORD last=GetLastError();FindVolumeClose(it);
    if(success && last!=ERROR_NO_MORE_FILES){error=L"Volume enumeration interrupted: "+error_text(last);return false;}
    return success;
}
bool match_drive(const Drive& old){
    Drive fresh;
    return query_disk(old.disk_number,fresh) && fresh.usb_bus && fresh.removable &&
           fresh.capacity==old.capacity && fresh.model==old.model && fresh.serial==old.serial &&
           safe_system_check(old.disk_number);
}
Result cancelled(){return {false,L"Cancelled. USB contents may be incomplete; recreate media before use."};}
}
std::vector<Drive> detect_removable_usb(){
    std::vector<Drive> found;
    // An explicit maximum avoids enumerating unknown device object paths indefinitely.
    for(DWORD num=0;num<128;num++){
        Drive d;
        if(query_disk(num,d) && d.usb_bus && d.removable && safe_system_check(num))found.push_back(std::move(d));
    }
    return found;
}
std::wstring describe(const Drive& d){
    std::ostringstream size;size<<human_size(d.capacity);
    auto str=size.str();
    return L"Disk "+std::to_wstring(d.disk_number)+L"  |  "+d.model+L"  |  "+std::wstring(str.begin(),str.end());
}
bool source_on_disk(const std::wstring& path,DWORD target,bool& unknown){
    // UNC files are remote and do not have a local physical-disk mapping.
    if(path.rfind(L"\\\\",0)==0 && path.rfind(L"\\\\?\\",0)!=0 && path.rfind(L"\\\\.\\",0)!=0){unknown=false;return false;}
    bool same=false;
    unknown=!disk_for_path(path,target,same);
    return same;
}
Result flash_and_verify(const Drive& selected,const std::wstring& image_path,const ImageInfo& info,
                        const std::atomic_bool& cancel,const Progress& progress){
    if(cancel.load())return cancelled();
    if(!match_drive(selected))return {false,L"USB identity changed or Windows system disk check failed."};
    bool unknown=false;
    bool same=source_on_disk(image_path,selected.disk_number,unknown);
    if(unknown)return {false,L"Cannot determine which disk contains the source image. Aborting safely."};
    SafetyInput policy{selected.usb_bus,selected.removable,false,same,true,selected.capacity,info};
    auto decision=assess(policy);
    if(!decision.allowed)return {false,std::wstring(decision.reason.begin(),decision.reason.end())};
    Handle source(CreateFileW(image_path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr));
    if(!source.valid())return {false,L"Cannot open image: "+error_text()};
    // Resolve the opened handle, not the supplied pathname: blocks symlinks/junctions
    // leading to a source image that actually resides on the target USB drive.
    std::vector<wchar_t> canonical(32768);
    DWORD path_count=GetFinalPathNameByHandleW(source.get(),canonical.data(),static_cast<DWORD>(canonical.size()),FILE_NAME_NORMALIZED);
    if(path_count==0 || path_count>=canonical.size())return {false,L"Could not resolve canonical image path."};
    bool final_unknown=false;
    if(source_on_disk(canonical.data(),selected.disk_number,final_unknown)||final_unknown)
        return {false,L"Source image is on the USB target or its physical disk is unknown."};
    LARGE_INTEGER current{};
    if(!GetFileSizeEx(source.get(),&current)||current.QuadPart<=0||static_cast<std::uint64_t>(current.QuadPart)!=info.bytes)
        return {false,L"Image size changed after inspection."};
    std::vector<Handle> locked;
    std::wstring lock_error;
    if(!lock_volumes(selected.disk_number,locked,lock_error))return {false,lock_error};
    if(cancel.load())return cancelled();
    if(!match_drive(selected))return {false,L"USB device changed before writing."};
    Handle target(CreateFileW(physical(selected.disk_number).c_str(),GENERIC_READ|GENERIC_WRITE,
                              FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_WRITE_THROUGH,nullptr));
    if(!target.valid())return {false,L"Cannot open target for writing: "+error_text()};
    if(!match_drive(selected))return {false,L"USB identity changed after opening the target."};
    constexpr DWORD chunk_size=1024*1024;
    std::vector<BYTE> image(chunk_size),disk(chunk_size);
    std::uint64_t done=0;unsigned last=101;
    auto announce=[&](unsigned pct,const std::wstring& status){if(pct!=last){last=pct;progress(pct,status);}};
    announce(0,L"Writing image to USB...");
    while(done<info.bytes){
        if(cancel.load())return cancelled();
        DWORD wanted=static_cast<DWORD>(std::min<std::uint64_t>(chunk_size,info.bytes-done));
        DWORD nread=0,nwrite=0;
        if(!ReadFile(source.get(),image.data(),wanted,&nread,nullptr)||nread!=wanted)
            return {false,L"Image read failed: "+error_text()};
        if(!WriteFile(target.get(),image.data(),wanted,&nwrite,nullptr)||nwrite!=wanted)
            return {false,L"USB write failed: "+error_text()};
        done+=wanted;
        announce(static_cast<unsigned>(done*50/info.bytes),L"Writing image to USB...");
    }
    if(!FlushFileBuffers(target.get()))return {false,L"Could not flush USB writes: "+error_text()};
    LARGE_INTEGER zero{};
    if(!SetFilePointerEx(source.get(),zero,nullptr,FILE_BEGIN)||!SetFilePointerEx(target.get(),zero,nullptr,FILE_BEGIN))
        return {false,L"Could not rewind devices for verification."};
    announce(50,L"Reading USB back and verifying every byte...");
    done=0;
    while(done<info.bytes){
        if(cancel.load())return cancelled();
        DWORD wanted=static_cast<DWORD>(std::min<std::uint64_t>(chunk_size,info.bytes-done));
        DWORD n1=0,n2=0;
        if(!ReadFile(source.get(),image.data(),wanted,&n1,nullptr)||n1!=wanted)
            return {false,L"Image read failed during verification: "+error_text()};
        if(!ReadFile(target.get(),disk.data(),wanted,&n2,nullptr)||n2!=wanted)
            return {false,L"USB read-back failed: "+error_text()};
        if(!std::equal(image.begin(),image.begin()+wanted,disk.begin()))
            return {false,L"Verification FAILED: USB bytes differ from the image. Do not use this USB."};
        done+=wanted;
        announce(50+static_cast<unsigned>(done*50/info.bytes),L"Reading USB back and verifying every byte...");
    }
    return {true,L"Write and full byte-for-byte verification completed. Bootability depends on the image and hardware."};
}
Result sha256(const std::wstring& path,const std::atomic_bool& cancel,const Progress& progress){
    Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr));
    if(!file.valid())return {false,L"Cannot open file for hashing: "+error_text()};
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(file.get(),&size)||size.QuadPart<=0)return {false,L"Invalid image file size."};
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    NTSTATUS code=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    if(code<0)return {false,L"Windows cryptographic provider unavailable."};
    struct AlgGuard {BCRYPT_ALG_HANDLE& h;~AlgGuard(){if(h)BCryptCloseAlgorithmProvider(h,0);}} alg_guard{alg};
    DWORD obj_size=0,cb=0;
    if(BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&obj_size),sizeof(obj_size),&cb,0)<0)
        return {false,L"Cannot initialize SHA-256."};
    std::vector<UCHAR> obj(obj_size);
    if(BCryptCreateHash(alg,&hash,obj.data(),static_cast<ULONG>(obj.size()),nullptr,0,0)<0)
        return {false,L"Cannot create SHA-256 hash."};
    struct HashGuard {BCRYPT_HASH_HANDLE& h;~HashGuard(){if(h)BCryptDestroyHash(h);}} hash_guard{hash};
    std::vector<UCHAR> b(1024*1024);
    std::uint64_t read_total=0;unsigned last=101;
    while(read_total<static_cast<std::uint64_t>(size.QuadPart)){
        if(cancel.load())return cancelled();
        DWORD wanted=static_cast<DWORD>(std::min<std::uint64_t>(b.size(),static_cast<std::uint64_t>(size.QuadPart)-read_total));
        DWORD got=0;
        if(!ReadFile(file.get(),b.data(),wanted,&got,nullptr)||got!=wanted)return {false,L"Image read failed during SHA-256."};
        if(BCryptHashData(hash,b.data(),got,0)<0)return {false,L"SHA-256 calculation failed."};
        read_total+=got;
        unsigned pct=static_cast<unsigned>(read_total*100/static_cast<std::uint64_t>(size.QuadPart));
        if(pct!=last){last=pct;progress(pct,L"Calculating SHA-256...");}
    }
    std::array<UCHAR,32> digest{};
    if(BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)<0)return {false,L"Cannot finalize SHA-256."};
    constexpr wchar_t hex[]=L"0123456789abcdef";
    std::wstring result=L"SHA-256: ";
    for(auto x:digest){result.push_back(hex[x>>4]);result.push_back(hex[x&15]);}
    return {true,result};
}
}
