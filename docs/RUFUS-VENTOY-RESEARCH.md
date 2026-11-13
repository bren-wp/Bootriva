# BOOTRIVA — Rufus and Ventoy interoperability and licence review

Research date: 2026-11-13

| Capability | BOOTRIVA v0.2.6 | Rufus | Ventoy |
| --- | --- | --- | --- |
| Native Windows UI | WinUI 3 frontend + C++ | Windows native | Native/web variants |
| Raw IMG and hybrid ISO flashing | Experimental; verify every written byte | Yes | Image booting rather than raw copy workflow |
| Windows install USB | Classic experimental assistant only | Yes | Boot ISO directly |
| SHA-256 | Native C++ engine | Yes | Image management |
| Multi-ISO USB | Read-only library inspector | One image at a time | Yes |
| UEFI / BIOS boot | Not yet hardware validated | Yes | Yes |
| Windows To Go | Not implemented | Yes | Image boot workflows |
| Bad-block testing | Not implemented | Yes | N/A |
| Secure Boot compatibility | Not validated | Supported media dependent | Supported configurations |

## Copyright and dependencies

- Rufus source: https://github.com/pbatard/rufus — GPLv3 (see LICENSE.txt).
- Ventoy source: https://github.com/ventoy/Ventoy — GPLv3 (see COPYING).
- BOOTRIVA core: MIT, copyright BRENDIGO.

**No Rufus or Ventoy source code, bootloader binaries, ISO images or proprietary files are copied into BOOTRIVA as part of this release.** Design goals and public behavior can inform independently authored functionality. Copying GPLv3 implementation code into the MIT-distributed core would require substantial licensing and redistribution review, with retention of original attribution and GPL obligations. A distinct GPL-compatible module may be explored after review. Upstream boot media may also contain separately licensed dependencies, firmware components and security-signing requirements.

## Forward plan

1. Validate current raw writer on disposable USB hardware.
2. Move classic Windows Setup media wizard into dedicated natively validated WinUI command path with secure firmware checks.
3. Implement an isolated, testable multiboot layout, firmware menu and image manifest, then validate BIOS, UEFI and Secure Boot.
4. Provide ISO checksum authenticity, bad-block detection, GPT/MBR and filesystem compatibility.
5. Implement Windows To Go only after verified deployment/servicing tests.
6. Add independent benchmark and boot compatibility report against public Rufus/Ventoy reference test media.

The labels *supported* or *better* must only be used after direct feature and hardware comparison, not based on screenshots or compilation alone.
