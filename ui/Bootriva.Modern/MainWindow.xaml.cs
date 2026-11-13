using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.Principal;
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
    private string? _imagePath;
    private NativeBridge.DiskImage? _image;
    private NativeBridge.UsbDrive[] _drives = [];
    private IntPtr _task = IntPtr.Zero;
    private bool _busy;
    private bool _nativeAvailable;
    private readonly bool _admin;

    public MainWindow()
    {
        InitializeComponent();
        _admin = HasAdministratorRights();
        AppWindow.SetPresenter(AppWindowPresenterKind.Overlapped);
        var screen = DisplayArea.GetFromWindowId(AppWindow.Id,DisplayAreaFallback.Primary).WorkArea;
        int width=Math.Max(350,Math.Min(740,screen.Width-48));
        int height=Math.Max(420,Math.Min(780,screen.Height-48));
        AppWindow.Resize(new SizeInt32(width,height));
        AppWindow.Move(new PointInt32(screen.X+(screen.Width-width)/2,screen.Y+(screen.Height-height)/2));
        AppWindow.Closing += (_,args) => {
            if(!_busy)return;
            args.Cancel=true;
            SetStatus("Operacija traje. Otkazivanje je moguće, ali zatvaranje mora pričekati kraj.");
        };
        try {
            _nativeAvailable=NativeBridge.br_abi_version()==NativeBridge.ExpectedAbi;
            if(!_nativeAvailable)SetStatus("Nespojiva nativna biblioteka. Zapisivanje blokirano.");
        }
        catch(Exception ex) {
            SetStatus("Nativna biblioteka nije dostupna: "+ex.Message);
        }
        PrivilegeInfo.Message = !_nativeAvailable
            ? "Nativni USB pogon nije dostupan. Provjeri isporučeni moderni ZIP."
            : _admin
                ? "C++ pogon verificira disk prije RAW zapisa i provjerava svaki bajt čitanjem natrag. Koristi samo testni USB."
                : "Analiza i SHA-256 dostupni su bez administratora. Za RAW zapis ponovno pokreni program s administratorskim ovlastima.";
        ElevateButton.Visibility = _admin ? Visibility.Collapsed : Visibility.Visible;
        RefreshActions();
        _=RefreshListAsync();
    }

    private static bool HasAdministratorRights(){
        using var id=WindowsIdentity.GetCurrent();
        return new WindowsPrincipal(id).IsInRole(WindowsBuiltInRole.Administrator);
    }
    private void SetStatus(string text)=>StatusText.Text=text;

    private void RefreshActions(){
        bool canInspect=_nativeAvailable && !_busy && _imagePath!=null;
        AnalyzeButton.IsEnabled=canInspect;
        HashButton.IsEnabled=canInspect;
        RefreshButton.IsEnabled=!_busy;
        ElevateButton.IsEnabled=!_busy;
        int selection=DriveCombo.SelectedIndex;
        FlashButton.IsEnabled=_nativeAvailable && _admin && !_busy
            && _image is {CanRawWrite:true} parsed
            && selection>=0 && selection<_drives.Length
            && parsed.Bytes<=_drives[selection].Capacity;
        CancelButton.IsEnabled=_busy && _task!=IntPtr.Zero;
        ImageSummary.Text=_image?.Summary ?? "ISO/IMG/BIN nije odabran.";
    }

    private void OpenOfficialIso(object sender,RoutedEventArgs e){
        int selected=DistributionCombo.SelectedIndex;
        if(selected<0 || selected>=OfficialUrls.Length)return;
        try{
            Process.Start(new ProcessStartInfo(OfficialUrls[selected].AbsoluteUri){UseShellExecute=true});
            SetStatus("Otvorena službena Microsoftova stranica. Preuzmi ISO i odaberi ga lokalno.");
        }catch(Exception ex){SetStatus("Otvaranje nije uspjelo: "+ex.Message);}
    }

    private async void BrowseImage(object sender,RoutedEventArgs e){
        if(_busy)return;
        try{
            var picker=new FileOpenPicker();
            InitializeWithWindow.Initialize(picker,WindowNative.GetWindowHandle(this));
            foreach(var ext in new[]{".iso",".img",".bin"})picker.FileTypeFilter.Add(ext);
            var selected=await picker.PickSingleFileAsync();
            if(selected==null)return;
            _imagePath=selected.Path;
            _image=null;
            ImagePath.Text=_imagePath;
            RefreshActions();
            await AnalyzeAsync();
        }catch(Exception ex){SetStatus("Odabir datoteke nije uspio: "+ex.Message);}
    }

    private async void AnalyzeImage(object sender,RoutedEventArgs e)=>await AnalyzeAsync();
    private async Task AnalyzeAsync(){
        if(_busy || !_nativeAvailable || _imagePath==null)return;
        _busy=true;RefreshActions();
        try{
            string path=_imagePath;
            _image=await Task.Run(()=>NativeBridge.Inspect(path));
            SetStatus(_image.Value.Summary);
        }catch(Exception ex){
            _image=null;
            SetStatus("Analiza nije uspjela: "+ex.Message);
        }finally{_busy=false;RefreshActions();}
    }

    private async void RefreshDrives(object sender,RoutedEventArgs e)=>await RefreshListAsync();
    private async Task RefreshListAsync(){
        if(!_nativeAvailable || _busy)return;
        _busy=true;RefreshActions();
        try{
            var fresh=await Task.Run(NativeBridge.EnumerateSafeDisks);
            _drives=fresh;
            DriveCombo.Items.Clear();
            foreach(var disk in fresh)DriveCombo.Items.Add(disk.Description);
            DriveCombo.SelectedIndex=-1; // Never auto-select a destructive target.
            SetStatus(fresh.Length==0?"Nema potvrđenih uklonjivih USB diskova.":
                      $"Pronađeno {fresh.Length} sigurnih fizičkih USB uređaja. Odaberi uređaj.");
        }catch(Exception ex){
            _drives=[];
            DriveCombo.Items.Clear();
            SetStatus("Provjera fizičkih USB diskova nije uspjela: "+ex.Message);
        }finally{_busy=false;RefreshActions();}
    }
    private void DriveSelected(object sender,SelectionChangedEventArgs e)=>RefreshActions();

    private void RequestElevation(object sender,RoutedEventArgs e){
        if(_busy || _admin)return;
        try{
            var path=Environment.ProcessPath ?? throw new InvalidOperationException("Putanja programa nije dostupna.");
            Process.Start(new ProcessStartInfo(path){UseShellExecute=true,Verb="runas"});
            Close();
        }catch(Win32Exception){
            SetStatus("Povišene ovlasti nisu odobrene. Pregled i hash ostaju dostupni.");
        }catch(Exception ex){SetStatus("Pokretanje administratora nije uspjelo: "+ex.Message);}
    }

    private async void CalculateHash(object sender,RoutedEventArgs e)=>await RunNativeAsync(false);
    private async void ConfirmAndFlash(object sender,RoutedEventArgs e){
        if(_busy || !_admin || !_nativeAvailable || _imagePath==null ||
           _image is not {CanRawWrite:true})return;
        int idx=DriveCombo.SelectedIndex;
        if(idx<0 || idx>=_drives.Length)return;
        var disk=_drives[idx];
        var prompt=new TextBox {PlaceholderText="BRISI",MaxLength=5};
        var details=new TextBlock {
            Text=$"Svi podaci na fizičkom USB disku {disk.DiskNumber} ({disk.Model}, " +
                 $"{disk.Capacity/1073741824.0:F1} GiB) bit će NEPOVRATNO obrisani. " +
                 "Koristi samo testni USB. Upiši BRISI za potvrdu:",
            TextWrapping=TextWrapping.Wrap
        };
        var body=new StackPanel{Spacing=12};
        body.Children.Add(details);body.Children.Add(prompt);
        var root=(Content as FrameworkElement)?.XamlRoot;
        if(root==null)return;
        var dialog=new ContentDialog{
            XamlRoot=root,Title="BOOTRIVA — upozorenje o brisanju",
            Content=body,PrimaryButtonText="Obriši i provjeri",
            CloseButtonText="Odustani",IsPrimaryButtonEnabled=false,
            DefaultButton=ContentDialogButton.Close
        };
        prompt.TextChanged+=(_,_)=>dialog.IsPrimaryButtonEnabled=
            string.Equals(prompt.Text,"BRISI",StringComparison.Ordinal);
        if(await dialog.ShowAsync()!=ContentDialogResult.Primary)return;
        await RunNativeAsync(true,disk);
    }

    private async Task RunNativeAsync(bool flash,NativeBridge.UsbDrive? disk=null){
        if(_busy || !_nativeAvailable || _imagePath==null)return;
        var task=NativeBridge.br_task_create();
        if(task==IntPtr.Zero){SetStatus("Nativna operacija se ne može pokrenuti.");return;}
        _task=task;_busy=true;OperationProgress.Value=0;RefreshActions();
        string file=_imagePath;
        NativeBridge.Progress progress=(value,message,_)=>{
            try{
                DispatcherQueue.TryEnqueue(()=>{
                    OperationProgress.Value=Math.Min(value,100u);
                    SetStatus(message+" "+value+"%");
                });
            }catch{ /* never throw across native callback */ }
        };
        try{
            SetStatus(flash?"Nativni USB zapis i verifikacija...":"SHA-256 provjera datoteke...");
            var result=await Task.Run(()=>flash
                ? NativeBridge.Flash(task,disk!.Value,file,progress)
                : NativeBridge.Hash(task,file,progress));
            if(result.Success)OperationProgress.Value=100;
            SetStatus((result.Success?"Završeno: ":"Prekinuto ili greška: ")+result.Message);
        }catch(Exception ex){
            SetStatus("Nativna operacija nije uspjela: "+ex.Message);
        }finally{
            NativeBridge.br_task_destroy(task);
            _task=IntPtr.Zero;_busy=false;RefreshActions();
        }
    }
    private void CancelOperation(object sender,RoutedEventArgs e){
        if(_task==IntPtr.Zero)return;
        NativeBridge.br_task_cancel(_task);
        CancelButton.IsEnabled=false;
        SetStatus("Otkazivanje zatraženo. USB može sadržavati nepotpune podatke.");
    }
    // Ventoy interoperability is deliberately non-destructive in this phase:
    // inspect existing multi-ISO folders only, never install a bootloader or
    // assume that an arbitrary folder is a bootable Ventoy USB.
    private async void BrowseVentoyFolder(object sender,RoutedEventArgs e)
    {
        if(_busy)return;
        try {
            var picker=new FolderPicker();
            InitializeWithWindow.Initialize(picker,WindowNative.GetWindowHandle(this));
            picker.FileTypeFilter.Add("*");
            var folder=await picker.PickSingleFolderAsync();
            if(folder==null)return;
            VentoyFiles.Items.Clear();
            VentoyPath.Text=folder.Path;
            var path=folder.Path;
            var files=await Task.Run(()=>Directory.EnumerateFiles(path,"*",SearchOption.TopDirectoryOnly)
                .Where(f=>new[]{".iso",".img",".wim",".vhd",".vhdx",".efi"}.Contains(
                    Path.GetExtension(f),StringComparer.OrdinalIgnoreCase))
                .OrderBy(Path.GetFileName,StringComparer.OrdinalIgnoreCase).Take(150).ToArray());
            foreach(var file in files) {
                var info=new FileInfo(file);
                VentoyFiles.Items.Add($"{info.Name}  ·  {info.Length/1048576.0:F0} MiB");
            }
            VentoyNotice.Text=files.Length==0
                ? "Nema pronađenih slika u korijenu odabrane mape."
                : $"Prikazano {files.Length} slika u odabranoj mapi. Ovo NE dokazuje da je USB bootabilan.";
            SetStatus("Ventoy mapa pregledana bez promjene datoteka ili particija.");
        } catch(Exception ex) {
            VentoyNotice.Text="Pregled multiboot mape nije uspio: "+ex.Message;
        }
    }

}
