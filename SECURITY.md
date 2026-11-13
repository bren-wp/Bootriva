# BOOTRIVA security and data-loss policy

Disk-writing tools have an unusually high consequence of failure. Never test with a USB device containing valuable data.

## Current enforced controls
- Device allowlist: Windows-reported USB bus AND removable media.
- Windows OS-volume disk guard and fail-closed detection when the OS volume is unknown. Other boot partitions may exist on separate disks.
- Local source file must not reside on the target disk; canonical opened file path is checked.
- Image size and user-visible target selection must match target identity at write time.
- Typed erase confirmation, volume locks, dismounts and byte-for-byte readback.
- No elevated subprocess shell commands, network requests, auto-downloads or telemetry.

## Known limitations
- Win32 UI and raw writer have not been compiled on Windows or hardware tested in this environment.
- Safety filtering may exclude USB SSDs classified as fixed disks.
- USB firmware can lie about its identity or advertised size; tests are needed on faulty/malicious USB hardware.
- The serial/model/size guard cannot fully defeat a malicious device impersonating another identical device.
- Do not rely on the raw writer for an optical-only ISO or bootloader installation.
- A power loss or removal during writing may corrupt the target medium.
- This release does not validate downloaded ISO authenticity; compare SHA-256 with a trusted vendor checksum.
- No fuzzing, SAST, dynamic Windows API analysis, installer signing or penetration testing completed yet.

## Release acceptance checklist
1. Windows x64 MSVC build and tests green.
2. Manual removable USB discovery tested across USB sticks and USB storage devices.
3. Confirm Windows system/boot disks are not exposed and insufficient-size drives are rejected.
4. Confirm source image on target drive, symlink/junction, changed target and mounted-volume failure all abort safely.
5. Test write/cancel/removal/power failure scenarios with expendable drives.
6. Verify image bytes and actual bootability on UEFI/BIOS hardware; bootability depends on the ISO.
7. Conduct independent security review, run static analyzers and check licenses.
8. Sign binaries and publish checksums for release artifacts.

Please report vulnerabilities privately through the project's security contact/repository security advisories once hosted.
