# BOOTRIVA roadmap

**0.2.x — in progress**

- [x] Native Windows UI and removable USB detection from 0.1.0
- [x] Image type validation, explicit destructive confirmation, raw image write/read-back and SHA-256 implementation
- [x] Separate experimental Windows ISO -> USB assistant with read-only preflight, FAT32, NTFS legacy BIOS mode, UEFI boot files, DISM large-WIM splitting
- [x] Windows build/test workflow with mandatory portable+setup artifact validation
- [ ] Test native app and USB wizard on disposable Windows hardware
- [ ] Integrate the guided Windows ISO installation path in native UI
- [ ] Full Windows installer compatibility matrix including secure boot and USB vendor variations
- [ ] Genuine Windows screenshot capture, image inspection and README gallery

**0.3.x**
- [ ] Optical-only Linux ISO extraction and bootloader integration with supported distributions
- [ ] Dedicated FAT32 and NTFS layout selection UI; report partition topology
- [ ] Secure download and official ISO verification
- [ ] Backup/restore and device-health checks

**0.4.x and beyond**
- [ ] Multiboot architecture with a supported boot manager (license review required)
- [ ] Windows To Go validation and Windows licensing requirements
- [ ] Accessibility, 200%-400% scaling, internationalization, compatibility and penetration tests
- [ ] Signed public stable releases after real hardware certification; unsigned builds must be labeled
