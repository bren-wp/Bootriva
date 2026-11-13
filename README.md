<div align="center">

<img src="assets/bootriva-logo.svg" width="490" alt="BOOTRIVA — Create. Flash. Boot." />

### Create. Flash. Boot.

**Create bootable USB media with control and confidence.**

🛡️ USB safety checks &nbsp;•&nbsp; 💿 ISO / IMG / BIN &nbsp;•&nbsp; 🔐 SHA-256 &nbsp;•&nbsp; ✅ Read-back verification

[**Download**](https://github.com/bren-wp/Bootriva/releases) • [**Windows ISO guide**](docs/WINDOWS-MEDIA.md) • [**Roadmap**](docs/ROADMAP.md)

**A BRENDIGO product** · [brendigo.com](https://brendigo.com)

</div>

---

## One USB creator. Zero unnecessary complexity.

BOOTRIVA pairs a focused native Windows interface with a deliberately defensive USB-writing engine. No account, adverts, or telemetry. Just a connected USB drive, your disk image, and clear progress and verification.

### ✨ What BOOTRIVA can do

| Capability | Status |
|---|---|
| Native Windows GUI with HR and EN | Implemented in source — Windows build verification pending |
| Detect removable USB media | Implemented in source |
| Inspect ISO9660, MBR and GPT image headers | Implemented and core-tested |
| Write raw IMG/BIN or pre-hybridized ISO | Implemented — real USB tests pending |
| Byte-for-byte read-back and SHA-256 checksum | Implemented — Windows tests pending |
| Create Windows 10/11 installation USB with FAT32 | Experimental **PowerShell assistant**, not yet in GUI |
| GPT+UEFI and MBR+BIOS modes | Experimental Windows media assistant |
| NTFS on supported legacy BIOS install media | Experimental Windows media assistant |
| Automatic large install.wim splitting with DISM | Experimental Windows media assistant |
| Multiboot and Windows To Go | Planned |

## 📦 Windows downloads

Every version-tagged build is configured to publish both **`BOOTRIVA-Portable.exe`** and **`BOOTRIVA-Setup.exe`**, plus **`SHA256SUMS.txt`**. A missing or failed binary/test prevents publishing the release. Until code signing and USB hardware qualification are complete, releases remain **unsigned prereleases**.

**Portable:** no installation required; run it as administrator. **Setup:** traditional installation and Start menu shortcuts.

## 🛠️ Startup UI fix
The Windows window-creation regression reported with `v0.2.0-preview.1`
is addressed in the current development branch. Win32 controls are now
initialized with the parent window handle during `WM_NCCREATE`, the background
does not overpaint child controls, and CI runs an explicit GUI-control smoke test.
**The fix is pending validation against a real Windows desktop screenshot.**

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
