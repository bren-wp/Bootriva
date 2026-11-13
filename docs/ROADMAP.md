# BOOTRIVA roadmap

**0.2.x — in progress**

- [x] Native Windows UI and removable USB detection from 0.1.0
- [x] Image type validation, explicit destructive confirmation, raw image write/read-back and SHA-256 implementation
- [x] Separate experimental Windows ISO -> USB assistant with read-only preflight, FAT32, NTFS legacy BIOS mode, UEFI boot files, DISM large-WIM splitting
- [x] Windows build/test workflow with mandatory portable+setup artifact validation
- [ ] Test native app and USB wizard on disposable Windows hardware
- [x] Integrate experimental guided Windows ISO assistant into native GUI via embedded resource (hardware validation pending)
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

## v0.2.2 ISO catalog and native UI refinement

- [x] Built-in curated list of official Microsoft Windows 11, Windows 10 and Windows Server 2025 Evaluation download pages; no copyrighted ISO binary redistribution.
- [x] Native UI sectioned into visual cards, revised spacing, logical-DPI layout with flexible activity log, and correct UI control discovery smoke tests.
- [ ] Capture real Windows screenshots at 100%, 150% and 200% scaling; refine font scaling based on those captures.
- [ ] Qualify BIOS/UEFI media with real removable USB hardware and verify Microsoft installer compatibility.

## v0.2.3 — Native compact adaptive desktop UI

- [x] Compact, centered normal window by default (work-area and display-DPI constrained; not maximized).
- [x] Responsive card layout: rows collapse into stacked controls on narrow windows.
- [x] Vertical scroll with mouse wheel and scrollbar to keep every setting reachable on short screens.
- [x] Native Win32 rounded cards/buttons, Windows 11 DWM dark caption and rounded-corner preference where supported.
- [x] Refresh Segoe UI control fonts on per-monitor DPI changes; extend Win32 GUI smoke tests for narrow viewport scrolling and initial window size.
- [ ] Confirm on hardware at 100%, 125%, 150%, 200% and 400% zoom, capture only genuine screenshots, test keyboard/AT navigation.

## v0.2.4 — Screenshot-based layout corrections

- [x] Separate scrollable page HWND from fixed header so scrolling cannot hide branding/navigation or place card content over it.
- [x] Align Win32 native inputs with a cohesive Windows 11 inspired light palette.
- [x] Reduce oversized cards and controls, ensure narrow action buttons have readable labels.
- [x] Replace bright default OS progress track with a compact custom native GDI control.
- [x] Add smoke regression for header position after scrolling to end of the page.
- [ ] Check screenshot before/after on a physical Windows desktop at multiple zoom factors and inspect contrast/accessibility.
- [ ] Review keyboard accessibility and native UI Automation semantics of custom controls.

## v0.2.5 — actual WinUI 3 modernization
- [x] Introduce Windows App SDK WinUI 3 XAML project, no Electron, no WebView.
- [x] Add official Microsoft catalogue, image picker, read-only disk preview and SHA-256 tool in WinUI.
- [x] Formalize hardware/safety gates before switching to the WinUI shell.
- [ ] Verify the WinUI app compiles, launches and packages on Windows CI.
- [ ] Implement a C++/WinUI safe bridge while preserving the existing USB engine.
- [ ] Design and validate real screenshots and accessible keyboard-navigation.

## v0.2.6 — WinUI/C++ safety integration and Rufus/Ventoy research

- [x] Introduce C ABI DLL with version handshake, native disk identity, cancellation and byte-by-byte verification.
- [x] Wire WinUI controls to native USB enumeration, image inspection, SHA-256, guarded raw writing with manual BRISI confirmation and progress.
- [x] Add read-only Ventoy-compatible ISO folder scanner without bootloader/partition modifications.
- [x] Build/test native bridge and package WinUI preview EXE with mandatory DLL in a ZIP.
- [ ] Independent native USB hardware write/read-back, UEFI/BIOS installer boot and hot-unplug matrix.
- [ ] WinUI Windows 10/11 installer USB creator, bootloader and filesystems with full identity rechecks.
- [ ] Build audited multiboot engine with explicit attribution, license review, Secure Boot and firmware validation.
- [ ] Compare against Rufus/Ventoy feature tests, including GPT/MBR, Windows To Go, Linux persistence and bad-block checks.
