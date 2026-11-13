# Creating a Windows installer USB (experimental)

The optical ISO installer pathway is provided as a **separate, opt-in PowerShell assistant** pending integration into the graphical BOOTRIVA wizard. It uses built-in Windows Storage cmdlets and DISM. It is *not* the raw/hybrid image writer.

## Requirements

Windows 10/11, PowerShell 5.1+, administrator privileges for actual writes, Windows ISO containing x64 boot files, and a disposable 8 GB+ USB device. All content on the selected USB is erased **only with** `-Execute -ConfirmErase ERASE`.

First inspect disk number with `Get-Disk`, then run validation (no write):

```powershell
powershell -NoProfile -File .\tools\create-windows-usb.ps1 -IsoPath C:\ISO\Windows11.iso -DiskNumber 3 -Firmware UEFI -Filesystem FAT32
```

Only after checking the exact disk identity and backing up data:

```powershell
powershell -NoProfile -File .\tools\create-windows-usb.ps1 -IsoPath C:\ISO\Windows11.iso -DiskNumber 3 -Firmware UEFI -Filesystem FAT32 -Execute -ConfirmErase ERASE
```

The script validates USB bus type and protects the system/boot disk, validates ISO contents before formatting, mounts the ISO read-only through Windows, prepares GPT+FAT32 for UEFI or MBR+FAT32/NTFS for legacy BIOS, copies setup files, and uses the installed Windows DISM to split a >4GB `install.wim` into supported `.swm` files for FAT32. For BIOS, it only executes a valid Microsoft-signed `bootsect.exe`. No arbitrary binaries from the ISO are run.

**Important limitations:** On UEFI we require FAT32; there is no UEFI:NTFS driver yet. The Windows formatter limit means USB sticks larger than ~32GB get a 30GB FAT32 installation partition; the remainder remains unused. Not all ISOs/firmware are compatible. A copied image is not proof of bootability. Hardware tests are mandatory before stable release.

References:
- https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/install-windows-from-a-usb-flash-drive
- https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/boot-to-uefi-mode-or-legacy-bios-mode
