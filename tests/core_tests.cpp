#include "bootriva/core.hpp"
#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace bootriva;
namespace fs=std::filesystem;
int main(){
 auto dir=fs::temp_directory_path()/"bootriva-tests";
 fs::create_directories(dir);
 auto make=[&](const char* name,bool mbr,bool iso){
   auto p=dir/name;
   std::array<char,34816> bytes{};
   if(mbr){bytes[510]=char(0x55);bytes[511]=char(0xaa);bytes[450]=char(0x83);bytes[454]=1;bytes[458]=8;}
   if(iso){bytes[32769]='C';bytes[32770]='D';bytes[32771]='0';bytes[32772]='0';bytes[32773]='1';}
   std::ofstream f(p,std::ios::binary);f.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));f.close();return p;
 };
 auto iso=inspect_image(make("optical.iso",false,true));
 assert(iso.iso9660 && !iso.hybrid_writable && iso.error.empty());
 auto hybrid=inspect_image(make("hybrid.iso",true,true));
 assert(hybrid.iso9660 && hybrid.mbr && hybrid.hybrid_writable);
 auto gpt_path=make("gpt.img",true,false);
 {std::fstream f(gpt_path,std::ios::in|std::ios::out|std::ios::binary);f.seekp(512);f.write("EFI PART",8);}
 auto gpt=inspect_image(gpt_path);assert(gpt.gpt && gpt.hybrid_writable);
 auto raw=inspect_image(make("raw.img",false,false));
 assert(raw.error.empty() && !raw.hybrid_writable);
 auto err=inspect_image(dir/"missing.iso");assert(!err.error.empty());
 SafetyInput s{true,true,false,false,true,65536,hybrid};
 assert(assess(s).allowed);
 s.image=iso;assert(!assess(s).allowed);
 s.image=raw;assert(assess(s).allowed);
 s.system_disk=true;assert(!assess(s).allowed);s.system_disk=false;
 s.same_source_disk=true;assert(!assess(s).allowed);s.same_source_disk=false;
 s.identity_matched=false;assert(!assess(s).allowed);s.identity_matched=true;
 s.removable=false;assert(!assess(s).allowed);s.removable=true;
 s.drive_bytes=1024;assert(!assess(s).allowed);s.drive_bytes=65536;
 s.image.bytes=70000;assert(!assess(s).allowed);
 assert(human_size(1024)=="1.0 KiB");
 fs::remove_all(dir);
 std::cout<<"Bootriva core tests passed\n";
}
