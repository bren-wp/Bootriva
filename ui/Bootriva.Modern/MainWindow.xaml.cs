using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using Windows.Graphics;
using Windows.Storage.Pickers;
using WinRT.Interop;

namespace Bootriva.Modern;

public sealed partial class MainWindow : Window
{
    private static readonly Uri[] OfficialUrls =
    [
        new("https://www.microsoft.com/hr-hr/software-download/windows11"),
        new("https://www.microsoft.com/en-us/software-download/windows10ISO"),
        new("https://www.microsoft.com/en-us/evalcenter/download-windows-server-2025")
    ];

    private string? _selectedImage;
    private bool _busy;

    public MainWindow()
    {
        InitializeComponent();
        // Work-area-safe Win11 window; no full-screen startup.
        AppWindow.Resize(new SizeInt32(740, 760));
        AppWindow.SetPresenter(AppWindowPresenterKind.Overlapped);
        var display = DisplayArea.GetFromWindowId(AppWindow.Id, DisplayAreaFallback.Primary);
        var area = display.WorkArea;
        AppWindow.Move(new PointInt32(
            area.X + Math.Max(0, (area.Width - 740) / 2),
            area.Y + Math.Max(0, (area.Height - 760) / 2)));
        RefreshDrivesList();
    }

    private void SetStatus(string message) => StatusText.Text = message;

    private void OpenOfficialIso(object sender, RoutedEventArgs e)
    {
        var index = DistributionCombo.SelectedIndex;
        if (index < 0 || index >= OfficialUrls.Length) return;
        Process.Start(new ProcessStartInfo(OfficialUrls[index].AbsoluteUri)
        {
            UseShellExecute = true
        });
        SetStatus("Otvorena službena Microsoftova stranica. Preuzmi ISO i potom odaberi datoteku.");
    }

    private async void BrowseImage(object sender, RoutedEventArgs e)
    {
        try
        {
            var picker = new FileOpenPicker();
            InitializeWithWindow.Initialize(picker, WindowNative.GetWindowHandle(this));
            picker.FileTypeFilter.Add(".iso");
            picker.FileTypeFilter.Add(".img");
            picker.FileTypeFilter.Add(".bin");
            var picked = await picker.PickSingleFileAsync();
            if (picked is null) return;
            _selectedImage = picked.Path;
            ImagePath.Text = _selectedImage;
            AnalyzeButton.IsEnabled = HashButton.IsEnabled = true;
            await AnalyzeCurrentAsync();
        }
        catch (Exception ex)
        {
            SetStatus("Odabir datoteke nije uspio: " + ex.Message);
        }
    }

    private async void AnalyzeImage(object sender, RoutedEventArgs e)
    {
        await AnalyzeCurrentAsync();
    }

    private async Task AnalyzeCurrentAsync()
    {
        if (_busy || _selectedImage is null) return;
        _busy = true;
        AnalyzeButton.IsEnabled = HashButton.IsEnabled = false;
        try
        {
            var path = _selectedImage;
            var text = await Task.Run(() => Inspect(path));
            ImageSummary.Text = text;
            SetStatus(text);
        }
        catch (Exception ex)
        {
            SetStatus("Provjera nije uspjela: " + ex.Message);
        }
        finally
        {
            _busy = false;
            AnalyzeButton.IsEnabled = HashButton.IsEnabled = _selectedImage is not null;
        }
    }

    private static string Inspect(string path)
    {
        var file = new FileInfo(path);
        if (!file.Exists || file.Length < 512)
            return "Odabrana slika ne postoji ili je manja od 512 B.";
        var ext = file.Extension.ToLowerInvariant();
        if (ext is not (".iso" or ".img" or ".bin"))
            return "Nepodržani nastavak. Odaberi ISO, IMG ili BIN.";
        var header = new byte[32774];
        using var stream = File.OpenRead(path);
        var read = stream.Read(header);
        bool mbr = read >= 512 && header[510] == 0x55 && header[511] == 0xAA;
        bool iso9660 = read >= 32774
            && System.Text.Encoding.ASCII.GetString(header,32769,5) == "CD001";
        bool gpt = read >= 520
            && System.Text.Encoding.ASCII.GetString(header,512,8) == "EFI PART";
        var kind = mbr || gpt ? "slika diska s MBR/GPT oznakom" :
                   iso9660 ? "ISO9660 optička slika" : "sirova slika diska";
        return $"Datoteka: {file.Length / 1048576.0:F1} MiB · {kind}. " +
               "Analiza je samo informativna i ne jamči mogućnost pokretanja.";
    }

    private async void CalculateHash(object sender, RoutedEventArgs e)
    {
        if (_selectedImage is null || _busy) return;
        _busy = true;
        AnalyzeButton.IsEnabled = HashButton.IsEnabled = false;
        try
        {
            SetStatus("Izračunava se SHA-256, datoteka se ne mijenja...");
            await using var file = new FileStream(
                _selectedImage, FileMode.Open, FileAccess.Read, FileShare.Read,
                1024 * 1024, FileOptions.SequentialScan | FileOptions.Asynchronous);
            var bytes = await SHA256.HashDataAsync(file);
            SetStatus("SHA-256: " + Convert.ToHexString(bytes));
            ImageSummary.Text = "SHA-256: " + Convert.ToHexString(bytes);
            OperationProgress.Value = 100;
        }
        catch (Exception ex)
        {
            SetStatus("SHA-256 provjera nije uspjela: " + ex.Message);
        }
        finally
        {
            _busy = false;
            AnalyzeButton.IsEnabled = HashButton.IsEnabled = true;
        }
    }

    private void RefreshDrives(object sender, RoutedEventArgs e) => RefreshDrivesList();

    private void RefreshDrivesList()
    {
        DriveCombo.Items.Clear();
        try
        {
            var removable = DriveInfo.GetDrives()
                .Where(x => x.DriveType == DriveType.Removable)
                .OrderBy(x => x.Name).ToArray();
            foreach (var drive in removable)
            {
                string size = drive.IsReady ? $"{drive.TotalSize / 1073741824.0:F1} GiB" : "Nije spreman";
                DriveCombo.Items.Add($"{drive.Name}  ·  {size}");
            }
            if (removable.Length > 0) DriveCombo.SelectedIndex = 0;
            SetStatus(removable.Length == 0
                ? "Nema prijavljenih prijenosnih logičkih diskova. USB-fiksni mediji još se ne prikazuju."
                : $"Pronađeno {removable.Length} prijenosnih logičkih diskova. Zapisivanje je onemogućeno.");
        }
        catch (Exception ex)
        {
            SetStatus("Pregled diskova nije uspio: " + ex.Message);
        }
    }
}
