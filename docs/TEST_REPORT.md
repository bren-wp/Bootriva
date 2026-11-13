# BOOTRIVA v0.1.0 — test report (2026-11-12)

## Verified in this environment (Linux x86_64)

- CMake C++20 core build: PASS
- Image inspection unit tests: PASS
- Safety decision unit tests: PASS
- Release-mode tests (assertions explicitly enabled): PASS
- ASan + UBSan tests: PASS
- Compiler warnings as errors for portable core: PASS

The tests exercise detection of hybrid/optical ISO, MBR, GPT, raw IMG,
missing file, removable/USB restrictions, source-disk guard, system disk
restriction, identity mismatch, and image larger than media.

## Not verified here

- Windows compiler/linker and resource script compilation.
- USB enumeration across Windows versions and drive models.
- UAC prompt and Windows UI behavior.
- Raw USB writer, volume locking/dismount, cancellation and byte-by-byte readback on physical devices.
- NSIS Setup.exe creation and digital signatures.
- Firmware bootability, ISO extraction, performance, accessibility and DPI scaling.

**Status: source development milestone, not production release.**
