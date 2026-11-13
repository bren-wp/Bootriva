<div align="center">

<img src="assets/bootriva-logo.svg" width="490" alt="BOOTRIVA — Create. Flash. Boot." />

### Create. Flash. Boot.

**Create bootable USB media with control and confidence.**

🛡️ USB safety checks &nbsp;•&nbsp; 💿 ISO / IMG / BIN &nbsp;•&nbsp; 🔐 SHA-256 &nbsp;•&nbsp; ✅ Read-back verification

[**Download**](https://github.com/bren-wp/Bootriva/releases) • [**Windows ISO guide**](docs/WINDOWS-MEDIA.md) • [**Roadmap**](docs/ROADMAP.md)

**A BRENDIGO product** · [brendigo.com](https://brendigo.com)

</div>

---

## WinUI 3 + hardened native C++ engine (v0.2.6)

The genuine WinUI 3 application now uses a versioned native C++ bridge for USB
discovery, image analysis, SHA-256, cancellation, confirmed raw disk-image
writes and **full byte-for-byte USB read-back verification**. Both the GUI and
the C++ engine independently validate physical disk identity before writing.

**RAW flashing is experimental and destructive**: it requires Administrator
privileges and a typed `BRISI` confirmation, and is restricted to eligible
removable USB devices. Non-hybrid optical ISO files are *not* raw flashed.
Windows Setup media creation remains an independent, experimental legacy UI
workflow until safely migrated.

A **Ventoy-compatible ISO library inspector** can read a chosen folder of ISO,
WIM, IMG, VHD/VHDX and EFI files. It does not alter partitions, install Ventoy,
or make the selected folder bootable. Its bootability is not guaranteed.

**Rufus and Ventoy are reference projects, not source-code dependencies.**
Their GPLv3 components are not incorporated into BOOTRIVA's MIT core.
Read [upstream comparison and licensing](docs/RUFUS-VENTOY-RESEARCH.md).

The WinUI bundle is now a ZIP because the mandatory native bridge DLL must
accompany the modern self-contained EXE. This prevents distributing a modern
EXE that cannot access the USB safety engine.

## Real Windows 11 interface — WinUI 3 preview (v0.2.5)

BOOTRIVA is transitioning to **Microsoft WinUI 3 and the Windows App SDK**:
real Fluent/XAML controls, compact adaptive cards, Windows native pickers,
native scrolling, clear status, no Electron, no HTML and no WebView.

**Three GitHub release binaries during transition:**
- **BOOTRIVA-Portable.exe** — current **C++ USB writer**, feature-preserving classic UI.
- **BOOTRIVA-Setup.exe** — current C++ installation package.
- **BOOTRIVA-WinUI3-Preview.zip** — self-contained native WinUI 3 EXE plus the
  mandatory C++ safety-engine DLL. Allows guarded raw writes and full read-back
  verification on an eligible removable USB, **not** general Windows ISO creation.

WinUI 3 does **not** replace the classic writer as the default release yet.
The native bridge now implements inspection, RAW write/verification and
cancellation, but Windows installer ISO preparation, tested UEFI/BIOS boot,
multiboot bootloader management and physical USB validation remain unfinished.

[WinUI 3 migration and testing gates](docs/WINUI3-MIGRATION.md) |
[Official Windows App SDK deployment guidance](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/unpackage-winui-app)

## One USB creator. Zero unnecessary complexity.

BOOTRIVA pairs a focused native Windows interface with a deliberately defensive USB-writing engine. No account, adverts, or telemetry. Just a connected USB drive, your disk image, and clear progress and verification.

### ✨ What BOOTRIVA can do

| Capability | Status |
|---|---|
| Native Windows GUI with HR and EN | Implemented; Windows CI GUI smoke tests |
| Detect removable USB media | Implemented in source |
| Inspect ISO9660, MBR and GPT image headers | Implemented and core-tested |
| Write raw IMG/BIN or pre-hybridized ISO | Implemented — real USB tests pending |
| Byte-for-byte read-back and SHA-256 checksum | Implemented; USB hardware validation pending |
| Create Windows 10/11 installation USB with FAT32 | Experimental wizard **integrated in the GUI** using the embedded assistant; hardware testing pending |
| GPT+UEFI and MBR+BIOS modes | GUI selection with experimental Windows media assistant |
| NTFS on supported legacy BIOS install media | Experimental Windows media assistant |
| Automatic large install.wim splitting with DISM | Experimental Windows media assistant |
| Multiboot and Windows To Go | Planned |

## 📦 Windows downloads

Every version-tagged build is configured to publish both **`BOOTRIVA-Portable.exe`** and **`BOOTRIVA-Setup.exe`**, plus **`SHA256SUMS.txt`**. A missing or failed binary/test prevents publishing the release. Until code signing and USB hardware qualification are complete, releases remain **unsigned prereleases**.

**Portable:** no installation required; run it as administrator. **Setup:** traditional installation and Start menu shortcuts.

## ISO library — directly inside BOOTRIVA

Choose from a built-in selection of legitimate, official Windows download
destinations. BOOTRIVA opens the selected Microsoft page in your browser and
lets you download the current ISO with Microsoft's choice of language/edition.
Then select your downloaded file in BOOTRIVA and choose the USB workflow.

- **Windows 11 x64** — official multi-edition Microsoft ISO page
- **Windows 10** — official legacy download page; standard support ended 2025-10-14
- **Windows Server 2025 Evaluation** — official 180-day Microsoft evaluation

ISO binary files are **not bundled** with BOOTRIVA: they are multi-gigabyte,
may need edition selection and licensing, and direct links can expire. The
server entry is a download reference, not a claim of certified server USB support.

### Screenshot-driven UI overhaul (v0.2.4)

The Windows 11 Fluent-inspired **light** interface replaces the visually inconsistent
dark v0.2.3 layout. It stays a native C++/Win32 desktop app.

- **Sticky header**: the BOOTRIVA title, BRENDIGO version and language switch
  remain visible while the content scrolls inside a dedicated clipped viewport.
- **Compact cards**: smaller vertical padding, consistent field heights and
  readable spacing. Narrow windows stack labels, choices and buttons cleanly.
- **Native controls**: OS-default Windows light comboboxes and edit fields now
  belong to a matching white/silver palette instead of clashing with a dark card.
- **Buttons and status**: rounded primary/secondary styles, abbreviated text that
  fits narrow controls, a slim branded progress bar and legible disabled states.
- **No full-screen startup**: opens in a centered normal window, not maximized.
- **Regression tests**: assert required controls, internal scrolling, a persistent
  visible header and the activity log at the end of the page.

The layout changes were driven by a real user screenshot showing clipped headings,
overlapping Windows USB controls, inconsistent control colors and poor spacing.
**Compiled GUI smoke tests are not a substitute for a real screenshot or hardware
review**; further refinement at 125%, 150%, 200% and 400% is ongoing.

### Compact native Windows 11-inspired interface (v0.2.3)

BOOTRIVA remains a **native C++20 Win32 executable** (no web runtime,
Electron or embedded Chromium). On launch, the application opens a compact
**660 × 720 logical-pixel normal window**, centered within the Windows desktop
working area. It never opens maximized by default.

The modern dark Windows interface offers:
- rounded Windows 11 title-bar corners where supported;
- rounded grouped cards and owner-drawn primary/secondary buttons;
- responsive vertical stacking on narrower widths;
- built-in vertical scrolling for smaller displays, including mouse wheel;
- per-monitor DPI-aware dimensions and typography;
- tested compact-launch and narrow-window control accessibility in Windows CI.

Only supported Windows desktop versions are targeted. UI inspiration is not a
claim of pixel-identical WinUI 3 controls. Real screenshots and physical
screen-reader/200% DPI tests are still needed.

### Refined native interface (v0.2.2)

The Windows app now separates source discovery, USB selection, image selection,
Windows setup and activity into dedicated dark cards, with improved spacing,
a resizable activity panel, clearer disabled-button states and a DPI-aware layout.
Real screenshot capture and physical Windows hardware testing are still required.

## 🪟 Windows 10 / 11 installation USB — experimental

Choose a **Windows 10/11 ISO**, connect a disposable USB device of at least 8 GiB, and select the installer mode in the new Windows USB section: **UEFI / FAT32**, **Legacy BIOS / FAT32**, or **Legacy BIOS / NTFS**. Then select **CREATE WINDOWS USB**. BOOTRIVA requires typed erasure confirmation before launching its embedded Windows PowerShell assistant with an independent device check.

The helper is embedded as a resource directly into the executable. **Both Portable and Setup have the same functionality; no sidecar script is necessary.** This path is distinct from raw image flashing and relies on Windows Storage cmdlets plus DISM splitting when install.wim exceeds the FAT32 file limit.

⚠️ This functionality is *experimental*, has not been qualified on physical Windows installation devices, and is not compatible with ordinary Linux optical ISO files. Check the console and verify the USB boots on representative hardware before deployment.

## 🛠️ Startup UI fix
The Windows window-creation regression reported with `v0.2.0-preview.1`
is addressed in the current development branch. Win32 controls are now
initialized with the parent window handle during `WM_NCCREATE`, the background
does not overpaint child controls, and CI runs an explicit GUI-control smoke test.
**Native automated Windows checks passed; a real Windows desktop screenshot and hardware validation are still pending.**

## 🪟 Interface gallery

**Real Windows screenshots only.** Screenshots will be added after a compiled application is run and captured on Windows and its identity verified. We intentionally do not show placeholder mockups as real application screens.

## ⚙️ Build from source

Windows with Visual Studio 2022+ C++ workload:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Linux core tests (GUI requires Windows):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## 🛡️ Safety by design

USB selection limits targets to removable USB-bus devices, excludes the Windows system disk, checks device identity and avoids source/target collisions. Destructive raw writes require a typed confirmation. The new Windows ISO assistant is validation-only unless both `-Execute` and `-ConfirmErase ERASE` are specified. All USB data is lost after explicit confirmation; use only spare drives in this early phase.

[Security policy](SECURITY.md) • [Windows installation instructions](docs/WINDOWS-MEDIA.md)

---

<div align="center"><sub>© 2026 BRENDIGO · Made with focus and precision · <a href="https://brendigo.com">brendigo.com</a></sub></div>
