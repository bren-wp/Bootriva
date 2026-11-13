#include "bootriva/bridge.h"
#include <Windows.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
int main(){
    auto expect=[](bool value,const char* label)->bool{
        if(!value)std::cerr<<"Bridge safety test failure: "<<label<<'\n';
        return value;
    };
    if(!expect(br_abi_version()==0x00020006u,"ABI version"))return 1;
    BrImage empty{};
    if(!expect(br_inspect_image(nullptr,&empty)==0 && empty.error[0]!=0,
               "invalid image refusal"))return 2;
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
    if(!expect(br_inspect_image(temp.c_str(),&image)==1 &&
               image.bytes==1024 && image.mbr==1,"image header"))return 3;
    auto* task=br_task_create();
    if(!expect(task!=nullptr,"create native task"))return 4;
    wchar_t message[512]{};
    const bool hash=br_run_sha256(task,temp.c_str(),nullptr,nullptr,message,512)==1;
    if(!expect(hash && std::wstring(message).rfind(L"SHA-256: ",0)==0,"native sha256"))return 5;
    BrDrive fake{};fake.disk_number=0;
    const bool refused=br_run_flash(task,&fake,temp.c_str(),L"NO",
        nullptr,nullptr,message,512)==0;
    if(!expect(refused && std::wstring(message).find(L"BRISI")!=std::wstring::npos,
               "invalid destructive confirmation"))return 6;
    br_task_cancel(task);
    br_task_destroy(task);
    std::error_code ec;
    std::filesystem::remove(temp,ec);
    return 0;
}
