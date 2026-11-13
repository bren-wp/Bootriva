using System.Runtime.InteropServices;
using System.Text;

namespace Bootriva.Modern;

/// <summary>A versioned Win64 C ABI. The C++ layer owns destructive-operation safety.</summary>
internal static class NativeBridge
{
    private const string Library = "bootriva_bridge.dll";
    internal const uint ExpectedAbi = 0x00020006;

    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    internal struct UsbDrive
    {
        public uint DiskNumber;
        public ulong Capacity;
        public uint Removable;
        public uint UsbBus;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=128)]
        public string Model;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=128)]
        public string Serial;
        public readonly string Description =>
            $"Fizički disk {DiskNumber} · {Model?.Trim()} · {Capacity/1073741824.0:F1} GiB";
    }

    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    internal struct DiskImage
    {
        public ulong Bytes;
        public uint Mbr;
        public uint Gpt;
        public uint Iso9660;
        public uint IsIsoExtension;
        public uint HybridWritable;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=256)]
        public string Error;
        public readonly bool Valid => string.IsNullOrEmpty(Error);
        public readonly bool CanRawWrite => Valid && (IsIsoExtension == 0 || HybridWritable != 0);
        public readonly string Summary => !Valid ? Error :
            $"{Bytes/1048576.0:F1} MiB · " +
            (Gpt != 0 ? "GPT" : Mbr != 0 ? "MBR" : Iso9660 != 0 ? "ISO9660" : "RAW") +
            (CanRawWrite ? " · RAW zapis zahtijeva potvrdu" : " · samo optički ISO, bez RAW zapisa");
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet=CharSet.Unicode)]
    internal delegate void Progress(uint percentage,
        [MarshalAs(UnmanagedType.LPWStr)] string message, IntPtr context);

    [DllImport(Library, CallingConvention=CallingConvention.Cdecl)]
    internal static extern uint br_abi_version();

    [DllImport(Library, CallingConvention=CallingConvention.Cdecl)]
    private static extern uint br_list_usb([Out] UsbDrive[] drives, uint count);

    [DllImport(Library, CharSet=CharSet.Unicode, CallingConvention=CallingConvention.Cdecl)]
    private static extern int br_inspect_image(
        [MarshalAs(UnmanagedType.LPWStr)] string path, out DiskImage image);

    [DllImport(Library, CallingConvention=CallingConvention.Cdecl)]
    internal static extern IntPtr br_task_create();

    [DllImport(Library, CallingConvention=CallingConvention.Cdecl)]
    internal static extern void br_task_cancel(IntPtr task);

    [DllImport(Library, CallingConvention=CallingConvention.Cdecl)]
    internal static extern void br_task_destroy(IntPtr task);

    [DllImport(Library, CharSet=CharSet.Unicode, CallingConvention=CallingConvention.Cdecl)]
    private static extern int br_run_sha256(
        IntPtr task, [MarshalAs(UnmanagedType.LPWStr)] string file,
        Progress callback, IntPtr context, StringBuilder result, uint length);

    [DllImport(Library, CharSet=CharSet.Unicode, CallingConvention=CallingConvention.Cdecl)]
    private static extern int br_run_flash(
        IntPtr task, ref UsbDrive drive,
        [MarshalAs(UnmanagedType.LPWStr)] string image,
        [MarshalAs(UnmanagedType.LPWStr)] string confirmation,
        Progress callback, IntPtr context, StringBuilder result, uint length);

    internal static UsbDrive[] EnumerateSafeDisks()
    {
        var buffer = new UsbDrive[128];
        var count = br_list_usb(buffer, (uint)buffer.Length);
        if (count > buffer.Length) throw new InvalidOperationException("Nedopušten broj USB uređaja.");
        return buffer.Take((int)count).ToArray();
    }

    internal static DiskImage Inspect(string path)
    {
        br_inspect_image(path, out var info);
        return info;
    }

    internal static (bool Success,string Message) Hash(IntPtr task, string file, Progress callback)
    {
        var text = new StringBuilder(512);
        bool ok = br_run_sha256(task,file,callback,IntPtr.Zero,text,(uint)text.Capacity) == 1;
        GC.KeepAlive(callback);
        return (ok,text.ToString());
    }

    internal static (bool Success,string Message) Flash(
        IntPtr task, UsbDrive drive, string file, Progress callback)
    {
        var text = new StringBuilder(512);
        // The typed confirmation dialog is separate. The native engine
        // re-enumerates disk identity and repeats its checks before writing.
        bool ok = br_run_flash(task,ref drive,file,"BRISI",callback,IntPtr.Zero,text,
                               (uint)text.Capacity) == 1;
        GC.KeepAlive(callback);
        return (ok,text.ToString());
    }
}
