#requires -Version 5.1
<#
BOOTRIVA Windows installation USB assistant.
Intentionally separate from raw image flashing. Runs in validation-only mode unless -Execute is specified.
No external executables from the untrusted ISO are launched.
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory)][ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })][string]$IsoPath,
  [Parameter(Mandatory)][ValidateRange(0,4095)][int]$DiskNumber,
  [ValidateSet('UEFI','BIOS')][string]$Firmware = 'UEFI',
  [ValidateSet('FAT32','NTFS')][string]$Filesystem = 'FAT32',
  [switch]$Execute,
  [ValidateSet('ERASE')][string]$ConfirmErase
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$mountedByUs = $false
$image = $null
function Abort([string]$reason) { throw "BOOTRIVA: $reason" }
function EligibleDisk([int]$number) {
  $disk = Get-Disk -Number $number -ErrorAction Stop
  if ($disk.BusType -ne 'USB' -or $disk.IsBoot -or $disk.IsSystem -or $disk.IsOffline) {
    Abort 'Target must be an online USB disk that does not contain the Windows boot/system volume.'
  }
  if ($disk.IsReadOnly -or $disk.Size -lt 8GB) { Abort 'Target is read-only or smaller than 8 GB.' }
  if (-not $disk.UniqueId) { Abort 'Target disk lacks a stable identity; refusing destructive operations.' }
  return $disk
}
if ($Firmware -eq 'UEFI' -and $Filesystem -ne 'FAT32') {
  Abort 'The standard Windows UEFI installer requires FAT32. NTFS-only UEFI is not supported.'
}
if ($Filesystem -eq 'NTFS' -and $Firmware -ne 'BIOS') {
  Abort 'NTFS is supported only in explicit legacy BIOS mode.'
}
$source = (Resolve-Path -LiteralPath $IsoPath).ProviderPath
if ([IO.Path]::GetExtension($source).ToLowerInvariant() -ne '.iso') { Abort 'Select an ISO file.' }
$target = EligibleDisk $DiskNumber
$identity = $target.UniqueId
# Reject a local ISO physically stored on the disk we are about to erase.
$sourcePartition = $null
$sourceDrive = [IO.Path]::GetPathRoot($source).TrimEnd('\').TrimEnd(':')
if ($sourceDrive -match '^[A-Za-z]$') {
  $sourcePartition = Get-Partition -DriveLetter $sourceDrive -ErrorAction SilentlyContinue
  if ($sourcePartition -and $sourcePartition.DiskNumber -eq $DiskNumber) {
    Abort 'The ISO file is located on the target disk.'
  }
}
try {
  $alreadyMounted = Get-DiskImage -ImagePath $source -ErrorAction SilentlyContinue
  if (-not $alreadyMounted -or -not $alreadyMounted.Attached) {
    $image = Mount-DiskImage -ImagePath $source -PassThru -ErrorAction Stop
    $mountedByUs = $true
  } else { $image = $alreadyMounted }
  $volume = $image | Get-Volume | Where-Object DriveLetter | Select-Object -First 1
  if (-not $volume) { Abort 'ISO could not be mounted with an accessible drive letter.' }
  $root = "$( $volume.DriveLetter ):\"
  $setupFiles = @('setup.exe', 'sources\boot.wim', 'bootmgr')
  foreach ($file in $setupFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $file))) {
      Abort "Not a supported Windows installation ISO (missing $file)."
    }
  }
  if ($Firmware -eq 'UEFI' -and -not (Test-Path -LiteralPath (Join-Path $root 'efi\boot\bootx64.efi'))) {
    Abort 'ISO lacks x64 UEFI boot files. This beta supports Windows x64 UEFI media.'
  }
  $wim = Join-Path $root 'sources\install.wim'
  $esd = Join-Path $root 'sources\install.esd'
  if (-not (Test-Path -LiteralPath $wim) -and -not (Test-Path -LiteralPath $esd)) {
    Abort 'ISO lacks sources/install.wim or sources/install.esd.'
  }
  $bigWim = (Test-Path -LiteralPath $wim) -and ((Get-Item -LiteralPath $wim).Length -gt 4GB - 1)
  $entries = Get-ChildItem -LiteralPath $root -Force -Recurse -File -ErrorAction Stop
  foreach ($file in $entries) {
    if ($Filesystem -eq 'FAT32' -and $file.Length -gt (4GB - 1) -and
       -not ($bigWim -and $file.FullName -eq $wim)) {
      Abort "FAT32 cannot store a file larger than 4 GB: $($file.FullName)"
    }
  }
  Write-Host "BOOTRIVA | $Firmware | $Filesystem | Disk $DiskNumber | $([math]::Round($target.Size/1GB,1)) GiB"
  Write-Host "Image: $source"
  Write-Host "Image source: $root"
  Write-Host "Large WIM requires split: $bigWim"
  if (-not $Execute) {
    Write-Host 'Validation passed. No USB data was changed. Run with -Execute -ConfirmErase ERASE to create media.'
    return
  }
  if ($ConfirmErase -ne 'ERASE') { Abort 'Explicit -ConfirmErase ERASE is mandatory for writes.' }
  $admin = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
  if (-not $admin.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Abort 'Administrator rights are required.'
  }
  $current = EligibleDisk $DiskNumber
  if ($current.UniqueId -ne $identity -or $current.Size -ne $target.Size -or $current.FriendlyName -ne $target.FriendlyName) {
    Abort 'USB identity changed after inspection.'
  }
  # Revalidate mounted ISO is not located on USB before destructive operation.
  if ($sourcePartition -and $sourcePartition.DiskNumber -eq $DiskNumber) { Abort 'ISO resides on target disk.' }
  Write-Warning "ERASING USB DISK $DiskNumber ($($current.FriendlyName)). ALL CONTENTS WILL BE LOST."
  Clear-Disk -Number $DiskNumber -RemoveData -RemoveOEM -Confirm:$false -ErrorAction Stop
  $style = if ($Firmware -eq 'UEFI') { 'GPT' } else { 'MBR' }
  Initialize-Disk -Number $DiskNumber -PartitionStyle $style -ErrorAction Stop
  # Windows' FAT32 formatter supports partition sizes up to around 32 GB.
  $partitionSize = if ($Filesystem -eq 'FAT32' -and $current.Size -gt 32GB) { 30GB } else { $null }
  $partition = if ($partitionSize) {
    New-Partition -DiskNumber $DiskNumber -Size $partitionSize -AssignDriveLetter -ErrorAction Stop
  } else {
    New-Partition -DiskNumber $DiskNumber -UseMaximumSize -AssignDriveLetter -ErrorAction Stop
  }
  if ($Firmware -eq 'BIOS') {
    Set-Partition -DiskNumber $DiskNumber -PartitionNumber $partition.PartitionNumber -IsActive $true -ErrorAction Stop
  }
  $partition | Format-Volume -FileSystem $Filesystem -NewFileSystemLabel 'BOOTRIVA' -Force -Confirm:$false -ErrorAction Stop | Out-Null
  $dest = "$( $partition.DriveLetter ):\"
  if (-not (Test-Path -LiteralPath $dest)) { Abort 'USB partition was formatted but cannot be accessed.' }
  Write-Host "Copying Windows Setup media to $dest"
  foreach ($item in (Get-ChildItem -LiteralPath $root -Force)) {
    if ($item.PSIsContainer) {
      $targetDir = Join-Path $dest $item.Name
      New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
      foreach ($entry in (Get-ChildItem -LiteralPath $item.FullName -Force -Recurse)) {
        $relative = $entry.FullName.Substring($root.Length)
        if ($bigWim -and $entry.FullName -eq $wim) { continue }
        $output = Join-Path $dest $relative
        if ($entry.PSIsContainer) { New-Item -ItemType Directory -Force -Path $output | Out-Null }
        else { Copy-Item -LiteralPath $entry.FullName -Destination $output -Force -ErrorAction Stop }
      }
    } else { Copy-Item -LiteralPath $item.FullName -Destination $dest -Force -ErrorAction Stop }
  }
  if ($bigWim) {
    $splitOutput = Join-Path $dest 'sources\install.swm'
    Write-Host 'Splitting >4 GB install.wim using the Windows DISM tool.'
    & "$env:SystemRoot\System32\dism.exe" /Split-Image "/ImageFile:$wim" "/SWMFile:$splitOutput" /FileSize:3800
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $splitOutput)) {
      Abort "DISM splitting failed with exit code $LASTEXITCODE; USB may be incomplete."
    }
  }
  if ($Firmware -eq 'BIOS') {
    # Never execute bootsect.exe from an untrusted ISO. Require a trusted Microsoft-signed copy.
    $bootsect = Join-Path $root 'boot\bootsect.exe'
    if (-not (Test-Path -LiteralPath $bootsect)) { Abort 'BIOS install needs bootsect.exe, missing in ISO.' }
    $sig = Get-AuthenticodeSignature -LiteralPath $bootsect
    if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notmatch 'Microsoft') {
      Abort 'The ISO bootsect.exe is not correctly signed by Microsoft.'
    }
    & $bootsect /nt60 "$( $partition.DriveLetter ):" /force
    if ($LASTEXITCODE -ne 0) { Abort "bootsect failed ($LASTEXITCODE)." }
  }
  if (-not (Test-Path -LiteralPath (Join-Path $dest 'sources\boot.wim'))) {
    Abort 'Verification failed: boot.wim is missing on USB.'
  }
  if ($Firmware -eq 'UEFI' -and -not (Test-Path -LiteralPath (Join-Path $dest 'efi\boot\bootx64.efi'))) {
    Abort 'Verification failed: EFI bootloader is missing on USB.'
  }
  Write-Host 'Windows installation USB copied. Firmware bootability must still be tested on real hardware.'
} finally {
  if ($mountedByUs -and $image) { Dismount-DiskImage -ImagePath $source -ErrorAction SilentlyContinue | Out-Null }
}
