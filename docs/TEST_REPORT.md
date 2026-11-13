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

## BOOTRIVA v0.2.1 preview feature verification

- Native GUI exposes UEFI/FAT32, legacy BIOS/FAT32 and legacy BIOS/NTFS selections for Windows 10/11 setup ISO media.
- The Windows helper is embedded in the .exe as a resource and passed in-memory to Windows PowerShell via UTF-16LE EncodedCommand. It cannot be overridden by a neighboring unsigned sidecar .ps1 file.
- Optical ISO installation is gated on explicit erasure confirmation and a separate PowerShell disk-identity check. No USB write occurs during GUI smoke tests.
- Required proof before stable: real USB and UEFI/BIOS boot testing, successful Windows installation, execution/error logging and representative screenshots.

## v0.2.2 official ISO catalog and UI

- New UI smoke asserts that all 3 official ISO entries and website launch control exist.
- The dropdown only launches first-party, static Microsoft web pages and never ships ISO binaries or executes remote code.
- New rounded card layout is calculated from logical 96-DPI units with movable/resizable log section; real Windows desktop and screen-reader tests pending.
- Windows GUI executable and smoke target now link shell32 for ShellExecuteW.
- Signing and hardware certification remain unverified; release is a prerelease only.

## v0.2.3 native responsive UI checks

The GUI smoke test checks the initial non-maximized window fits inside the monitor working area, that all required controls exist, and that the settings page scrolls on a short/narrow window. CI also builds and packages the native Windows application and verifies the installer and checksums. Visual pixel-perfect, screen-reader and hardware tests remain pending; do not advertise those as completed.

## v0.2.4 screenshot defect reproduction and checks

Input: actual Windows screenshot from the user (2026-11-13), demonstrating clipped top header, bright progress stripe, truncated 'KREIRAJ WINDOWS USB' text, mismatched native dropdown arrows, large empty cards, and scroll/overlap problems.

Correction: fixed header stays on parent window; card controls live inside child viewport, which clips scrolling at the header boundary and forwards Win32 control commands, paint/draw and status messages to the parent. A light Fluent palette harmonizes standard Windows widgets. Layout labels and action controls use compact dimensions; progress is painted by a separate native Win32 class.

Automated smoke: native startup controls, official ISO entries, nonmaximized initial window, narrow-screen scroll to activity log, and immobility of header during scrolling. Real Windows screenshots and high-DPI hardware verification remain open.
