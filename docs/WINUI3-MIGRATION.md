# BOOTRIVA — WinUI 3 migration

## Purpose
The next generation UI is developed in **WinUI 3 + Windows App SDK** inside `ui/Bootriva.Modern`. It is real, native Windows XAML controls, not CSS/HTML, Chromium, Electron, or owner-drawn Win32 cards.

## Current boundaries (v0.2.5)
The modern shell is a *read-only preview*, with:
- Official Microsoft Windows ISO edition catalogue (links to Microsoft; no ISO redistribution).
- Windows file picker for ISO/IMG/BIN.
- ISO9660/MBR/GPT *header hints* (not bootability proof).
- Asynchronous SHA-256 checksum from the selected image.
- Read-only detection of removable logical drives.
- Windows 11 native cards, InfoBars, ComboBoxes, theme resources, DPI-aware ScrollViewer and a compact window.

**No raw write, formatting, partitioning or bootloader installation in WinUI preview.** Those operations remain available in the existing classic C++ executable, which is still the production-functionality path until safety-validated migration. USB devices exposed as fixed drives may not appear in the preview.

### Why not switch main exe yet?
The WinUI preview does not yet provide feature parity. Switching the default EXE before correct device identity, source-target isolation, cancellation, raw write/read-back, Windows ISO preflight and hardware verification would be misleading and risk data loss.

## Migration gates
1. [ ] Build WinUI project reproducibly in Windows CI, test an unpackaged self-contained binary.
2. [ ] Test launch on a real Windows 11 desktop at 100%, 125%, 150%, 200% and 400% DPI.
3. [ ] Extract the C++ core to a versioned, hardened C ABI bridge. No PowerShell injection surfaces.
4. [ ] Preserve *all* destructive operation protections and independent disk identity checks.
5. [ ] Integrate safe raw image writing and SHA-256 verification with progress/cancel.
6. [ ] Connect Windows installation media workflow, UEFI/GPT and BIOS/MBR.
7. [ ] Test real USB hardware and installer boot with reproducible device matrix.
8. [ ] Replace primary portable/setup binaries with WinUI version **only after all safety gates pass**.

## Build
Windows with .NET 10 SDK + WinAppSDK workloads, NuGet restored:
```powershell
dotnet publish ui/Bootriva.Modern/Bootriva.Modern.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true
```

Windows App SDK self-contained single-file builds extract their runtime dependencies on first launch. They are **not** zero-extraction binaries. Signing is not configured; all artifacts are unsigned previews.

See Microsoft: https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/unpackage-winui-app

## v0.2.6 — native C++ ABI migration

- The native Win64 `bootriva_bridge.dll` is included next to the WinUI executable in a single release ZIP, with a version handshake (`0x00020006`).
- WinUI uses the existing protected C++ USB engine for eligible physical USB disk selection, image inspection, raw IMG/hybrid ISO flashing, byte-for-byte read-back and SHA-256.
- Requires explicit administrator elevation, no implicit USB selection and a typed `BRISI` confirmation; C++ independently checks the selected device again before writing.
- Cancelling an in-progress raw write may leave the USB unusable until recreated; WinUI prevents closing while a native operation runs.
- Includes a **read-only** Ventoy ISO-library inspector. Does not import GPL source or bootloaders.
- Remaining: actual physical hardware + Windows installer + Linux ISO boot testing; real screenshot and accessibility validation; WinUI Windows Setup USB builder; safe bootloader integration.
