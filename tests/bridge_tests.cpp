#include "bootriva/bridge.h"
#include <Windows.h>
#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
int main(){
    assert(br_abi_version()==0x00020006u);
    BrImage empty{};
    assert(br_inspect_image(nullptr,&empty)==0);
    assert(empty.error[0]!=0);
    const auto temp=std::filesystem::temp_directory_path()/
        (L"bootriva-bridge-smoke-"+std::to_wstring(GetCurrentProcessId())+L".img");
    {
        std::array<unsigned char,1024> bytes{};
        bytes[510]=0x55;bytes[511]=0xAA;
        bytes[446+4]=0x83;
        bytes[446+8]=1;bytes[446+12]=1;
        std::ofstream out(temp,std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    }
    BrImage image{};
    assert(br_inspect_image(temp.c_str(),&image)==1);
    assert(image.bytes==1024 && image.mbr==1);
    auto* task=br_task_create();
    assert(task);
    wchar_t message[512]{};
    assert(br_run_sha256(task,temp.c_str(),nullptr,nullptr,message,512)==1);
    assert(std::wstring(message).find(L"SHA-256: ")==0);
    BrDrive fake{};fake.disk_number=0;
    // Safety regression: missing/incorrect confirmation must be rejected
    // before opening or even enumerating physical disks.
    assert(br_run_flash(task,&fake,temp.c_str(),L"NO",nullptr,nullptr,message,512)==0);
    assert(std::wstring(message).find(L"BRISI")!=std::wstring::npos);
    br_task_cancel(task);
    br_task_destroy(task);
    std::error_code ec;
    std::filesystem::remove(temp,ec);
    return 0;
}
