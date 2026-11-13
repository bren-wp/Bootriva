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

## GUI startup regression (2026-11-13)

- Report: Windows release opens to a blank dark window with only a horizontal separator.
- Root cause: `app.hwnd` was assigned only after `CreateWindowExW` returned, but `WM_CREATE` executed synchronously before that and attempted to create child controls with a null parent HWND.
- Correction: initialize owner HWND during `WM_NCCREATE`; fail gracefully if required controls are absent; prevent parent painting over child windows.
- Regression: native `bootriva_ui_smoke` Windows CTest checks required visible controls and window title; no disk operation or admin elevation.
- Evidence still needed: successfully completed Windows Actions test and real desktop screenshot after rebuilding the application.
