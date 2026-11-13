#include "bootriva/core.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace bootriva {
namespace {
std::uint32_t le32(const unsigned char* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8u)
        | (std::uint32_t(p[2]) << 16u) | (std::uint32_t(p[3]) << 24u);
}
}
ImageInfo inspect_image(const std::filesystem::path& path) {
    ImageInfo r;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) { r.error="File does not exist or is not a regular file."; return r; }
    r.bytes=std::filesystem::file_size(path, ec);
    if (ec || r.bytes < 512) { r.error="Image is missing, unreadable, or smaller than 512 bytes."; return r; }
    auto ext=path.extension().string();
    std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if (ext!=".iso" && ext!=".img" && ext!=".bin") { r.error="Select an .iso, .img, or .bin image."; return r; }
    r.is_iso_extension=(ext==".iso");
    std::ifstream f(path,std::ios::binary);
    if (!f) {r.error="Cannot open image for reading."; return r;}
    std::array<unsigned char,512> sector{};
    f.read(reinterpret_cast<char*>(sector.data()),static_cast<std::streamsize>(sector.size()));
    if (f.gcount()!=static_cast<std::streamsize>(sector.size())) {r.error="Cannot read image header.";return r;}
    const bool sig = sector[510]==0x55u && sector[511]==0xAAu;
    if(sig) {
        for(std::size_t i=0; i<4; ++i) {
            const auto* p=sector.data()+446+16*i;
            if(p[4]!=0 && le32(p+8)>0 && le32(p+12)>0) {r.mbr=true;break;}
        }
    }
    if(r.bytes>=1024){
        std::array<char,8> g{};
        f.read(g.data(),static_cast<std::streamsize>(g.size()));
        r.gpt=(f.gcount()==8 && std::string(g.data(),g.size())=="EFI PART");
    }
    if(r.bytes>=32774){
        f.clear();f.seekg(32769,std::ios::beg);
        std::array<char,5> magic{};
        f.read(magic.data(),static_cast<std::streamsize>(magic.size()));
        r.iso9660=(f.gcount()==5 && std::string(magic.data(),magic.size())=="CD001");
    }
    // A conventional optical-only ISO is NOT safe to advertise as a bootable raw USB image.
    r.hybrid_writable=r.mbr||r.gpt;
    if (r.is_iso_extension && !r.iso9660 && !r.hybrid_writable)
        r.error="This ISO has neither an ISO9660 marker nor an MBR/GPT disk map.";
    return r;
}
SafetyResult assess(const SafetyInput& in) {
    if(!in.usb_bus || !in.removable) return {false,"Only removable USB disk devices are supported."};
    if(in.system_disk) return {false,"The Windows system disk is always protected."};
    if(in.same_source_disk) return {false,"Image is stored on the USB target."};
    if(!in.identity_matched) return {false,"USB identity changed since selection; refresh devices."};
    if(!in.image.error.empty()) return {false,in.image.error};
    if(in.image.bytes==0 || in.image.bytes>in.drive_bytes) return {false,"Image is larger than the USB device."};
    if(in.image.is_iso_extension && !in.image.hybrid_writable)
        return {false,"ISO is not a raw/hybrid disk image. Optical-only ISO needs extraction and bootloader setup (not yet implemented)."};
    return {true,in.image.is_iso_extension ? "Hybrid ISO raw write available." : "Raw image write available (bootability depends on the image)."};
}
std::string human_size(std::uint64_t n) {
    const char* units[]={"B","KiB","MiB","GiB","TiB"};
    double size=static_cast<double>(n);
    std::size_t i=0;
    while(size>=1024 && i<4){size/=1024;++i;}
    std::ostringstream out;
    out<<std::fixed<<std::setprecision(i==0 ? 0 : 1)<<size<<' '<<units[i];
    return out.str();
}
}
