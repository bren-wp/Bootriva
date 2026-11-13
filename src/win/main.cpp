#include "usb_win.hpp"
#include <Windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <iterator>
#include <array>
#include <utility>
#include <atomic>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")
using bootriva::ImageInfo;
using bootriva::SafetyInput;
using bootriva::win::Drive;
namespace {
constexpr int ID_DEV=100,ID_REFRESH=101,ID_IMAGE=102,ID_BROWSE=103,ID_HASH=104,ID_FLASH=105,ID_LANG=106;
constexpr int ID_PROGRESS=107,ID_STATUS=108,ID_LOG=109,ID_TITLE=110,ID_SUBTITLE=111,ID_L_DEVICE=112,ID_L_IMAGE=113,ID_INFO=114,ID_VERSION=115;
constexpr int ID_WINUSB=116,ID_MODE=117,ID_MODE_LABEL=118;
constexpr int ID_CATALOG=119,ID_CATALOG_LABEL=120,ID_CATALOG_OPEN=121,ID_CATALOG_NOTE=122;
constexpr UINT MSG_PROGRESS=WM_APP+11,MSG_DONE=WM_APP+12;
constexpr COLORREF BG=RGB(13,21,36),FG=RGB(235,243,250),MUTED=RGB(156,175,192),ACCENT=RGB(29,197,204),PANEL=RGB(24,36,56);
struct ProgressEvent {unsigned value;std::wstring text;};
struct DoneEvent {bool ok;std::wstring text;};
struct App {
    HWND hwnd{},dev{},refresh{},image{},browse{},hash{},flash{},winusb{},mode{},mode_label{},catalog{},catalog_label{},catalog_open{},catalog_note{},lang{},progress{},status{},log{},title{},subtitle{},device_label{},image_label{},info{},version{};
    HFONT font{},title_font{},small_font{};
    HBRUSH background{},panel{};
    std::vector<Drive> drives;
    std::wstring image_path;
    ImageInfo image_info;
    std::thread worker;
    std::atomic_bool cancel{false};
    bool busy=false,close_after=false,english=false,external_process=false;
    int scroll_y=0,content_height=780;
    std::array<std::pair<int,int>,5> card_ranges{};
    ~App(){cancel=true;if(worker.joinable())worker.join();if(font)DeleteObject(font);if(title_font)DeleteObject(title_font);if(small_font)DeleteObject(small_font);if(background)DeleteObject(background);if(panel)DeleteObject(panel);}
};
std::wstring tr(const App& a,const wchar_t* hr,const wchar_t* en){return a.english?en:hr;}
void settext(HWND h,const std::wstring& s){SetWindowTextW(h,s.c_str());}
void log_message(App& a,const std::wstring& msg){
    if(!a.log)return;
    int length=GetWindowTextLengthW(a.log);
    if(length>65536){SetWindowTextW(a.log,L"");length=0;}
    SendMessageW(a.log,EM_SETSEL,length,length);
    std::wstring line=msg+L"\r\n";
    SendMessageW(a.log,EM_REPLACESEL,FALSE,reinterpret_cast<LPARAM>(line.c_str()));
    SendMessageW(a.log,EM_SCROLLCARET,0,0);
}
void status(App& a,const std::wstring& msg){settext(a.status,msg);log_message(a,msg);}
HWND child(HWND p,LPCWSTR type,LPCWSTR title,DWORD style,int id){
    return CreateWindowExW(0,type,title,WS_CHILD|WS_VISIBLE|style,0,0,0,0,p,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
}
void font(HWND h,HFONT f){SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(f),TRUE);}
int scale(const App& a,int value){return MulDiv(value,static_cast<int>(GetDpiForWindow(a.hwnd)),96);}
void layout(App& a,int w,int h){
    if(!a.dev || w<=0 || h<=0)return;
    const int dpi=static_cast<int>(GetDpiForWindow(a.hwnd));
    const int width=MulDiv(w,96,dpi),height=MulDiv(h,96,dpi);
    const int outer=16,inner=outer+16,content=std::max(220,width-inner*2);
    const bool narrow=content<475;
    const int btn=162,gap=10;
    const int inputWidth=narrow?content:content-btn-gap;
    const int buttonX=narrow?inner:inner+inputWidth+gap;
    auto place=[&](HWND control,int x,int y,int cw,int ch){
        if(control)MoveWindow(control,scale(a,x),scale(a,y)-a.scroll_y,
                              scale(a,std::max(1,cw)),scale(a,std::max(1,ch)),TRUE);
    };
    // Header and scrollable cards are laid out in *logical* 96-DPI units.
    // Each group grows vertically when the window is narrow, rather than
    // allowing controls to overlap or become clipped.
    place(a.title,inner,14,std::max(180,content-173),35);
    place(a.subtitle,inner,49,std::max(180,content-115),24);
    place(a.version,inner+content-160,12,160,19);
    place(a.lang,inner+content-91,44,91,30);
    int y=90;
    const int catTop=y;
    place(a.catalog_label,inner,y+12,content,22);
    place(a.catalog,inner,y+42,inputWidth,220);
    place(a.catalog_open,buttonX,y+(narrow?82:41),narrow?content:btn,36);
    const int catNoteY=y+(narrow?125:85);
    place(a.catalog_note,inner,catNoteY,content,39);
    y=catNoteY+48;
    a.card_ranges[0]={catTop,y};
    y+=9;
    const int deviceTop=y;
    place(a.device_label,inner,y+12,content,22);
    place(a.dev,inner,y+42,inputWidth,220);
    place(a.refresh,buttonX,y+(narrow?82:41),narrow?content:btn,36);
    y+=(narrow?130:91);
    a.card_ranges[1]={deviceTop,y};
    y+=9;
    const int imageTop=y;
    place(a.image_label,inner,y+12,content,22);
    place(a.image,inner,y+42,inputWidth,36);
    place(a.browse,buttonX,y+(narrow?82:41),narrow?content:btn,36);
    const int actionsY=y+(narrow?132:90);
    const int actionWidth=(content-gap)/2;
    place(a.hash,inner,actionsY,actionWidth,37);
    place(a.flash,inner+actionWidth+gap,actionsY,actionWidth,37);
    y=actionsY+48;
    a.card_ranges[2]={imageTop,y};
    y+=9;
    const int mediaTop=y;
    place(a.mode_label,inner,y+12,content,22);
    place(a.mode,inner,y+42,inputWidth,220);
    place(a.winusb,buttonX,y+(narrow?82:41),narrow?content:btn,36);
    y+=(narrow?130:91);
    a.card_ranges[3]={mediaTop,y};
    y+=9;
    const int activityTop=y;
    place(a.info,inner,y+12,content,43);
    place(a.progress,inner,y+64,content,13);
    place(a.status,inner,y+86,content,25);
    const int logHeight=std::max(94,height-(y+132));
    place(a.log,inner,y+119,content,logHeight);
    y+=119+logHeight+17;
    a.card_ranges[4]={activityTop,y};
    a.content_height=y+14;
    const int priorScroll=a.scroll_y;
    const int maxScroll=std::max(0,scale(a,a.content_height)-h);
    a.scroll_y=std::clamp(a.scroll_y,0,maxScroll);
    // Re-run placement once if a resize shortened the scrollable document.
    if(a.scroll_y!=priorScroll){layout(a,w,h);return;}
    SCROLLINFO info{sizeof(info)};
    info.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
    info.nMin=0;
    info.nMax=std::max(0,scale(a,a.content_height)-1);
    info.nPage=static_cast<UINT>(h);
    info.nPos=a.scroll_y;
    SetScrollInfo(a.hwnd,SB_VERT,&info,TRUE);
}
void scroll_to(App& a,int desired){
    RECT client{};
    GetClientRect(a.hwnd,&client);
    const int maxScroll=std::max(0,scale(a,a.content_height)-static_cast<int>(client.bottom-client.top));
    int target=std::clamp(desired,0,maxScroll);
    if(target==a.scroll_y)return;
    a.scroll_y=target;
    layout(a,client.right-client.left,client.bottom-client.top);
    InvalidateRect(a.hwnd,nullptr,FALSE);
}
struct CatalogEntry {
    const wchar_t* title;
    const wchar_t* official_url;
    const wchar_t* note_hr;
    const wchar_t* note_en;
};
constexpr CatalogEntry official_catalog[]{
    {L"Windows 11  |  x64  |  Microsoft ISO",
     L"https://www.microsoft.com/hr-hr/software-download/windows11",
     L"Službeni Microsoft ISO. Izdanje i jezik odabiru se na Microsoftovoj stranici.",
     L"Official Microsoft ISO. Select edition and language on Microsoft's website."},
    {L"Windows 10  |  ISO  |  prethodna generacija",
     L"https://www.microsoft.com/en-us/software-download/windows10ISO",
     L"Standardna podrška za Windows 10 završila je 14. 10. 2025.",
     L"Standard Windows 10 support ended on October 14, 2025."},
    {L"Windows Server 2025  |  Evaluation ISO",
     L"https://www.microsoft.com/en-us/evalcenter/download-windows-server-2025",
     L"Evaluacija 180 dana; BOOTRIVA još nije certificirana za Server instalacijski USB.",
     L"180-day evaluation; BOOTRIVA Server installation media is not yet certified."}
};
void update_catalog_note(App& a){
    if(!a.catalog_note || !a.catalog)return;
    int id=static_cast<int>(SendMessageW(a.catalog,CB_GETCURSEL,0,0));
    if(id<0 || id>=static_cast<int>(std::size(official_catalog)))return;
    const auto& entry=official_catalog[id];
    settext(a.catalog_note,a.english?entry.note_en:entry.note_hr);
}
void open_official_iso_catalog(App& a){
    if(a.busy)return;
    int id=static_cast<int>(SendMessageW(a.catalog,CB_GETCURSEL,0,0));
    if(id<0 || id>=static_cast<int>(std::size(official_catalog)))return;
    HINSTANCE result=ShellExecuteW(a.hwnd,L"open",official_catalog[id].official_url,nullptr,nullptr,SW_SHOWNORMAL);
    if(reinterpret_cast<INT_PTR>(result)<=32)
        status(a,tr(a,L"Microsoftovu stranicu nije moguće otvoriti.",L"Cannot open the Microsoft website."));
    else
        status(a,tr(a,L"Preuzmi ISO s Microsoftove stranice, zatim odaberi datoteku u BOOTRIVA-i.",
                      L"Download the ISO from Microsoft, then choose the file in BOOTRIVA."));
}
void update_buttons(App& a){
    EnableWindow(a.refresh,!a.busy);EnableWindow(a.dev,!a.busy);EnableWindow(a.browse,!a.busy);
    EnableWindow(a.hash,!a.busy && a.image_info.error.empty() && !a.image_path.empty());
    bool ready=!a.image_path.empty() && a.image_info.error.empty() && !a.drives.empty();
    EnableWindow(a.flash,(a.busy && !a.external_process) || (!a.busy && ready && (!a.image_info.is_iso_extension || a.image_info.hybrid_writable)));
    settext(a.flash,tr(a,a.busy?(a.external_process?L"PRIČEKAJ":L"ZAUSTAVI"):L"ZAPIŠI USB",
                           a.busy?(a.external_process?L"PLEASE WAIT":L"CANCEL"):L"WRITE USB"));
    EnableWindow(a.winusb,!a.busy && ready && a.image_info.is_iso_extension);
    EnableWindow(a.mode,!a.busy);
    EnableWindow(a.catalog,!a.busy);
    EnableWindow(a.catalog_open,!a.busy);
    EnableWindow(a.lang,!a.busy);
}
void translate(App& a){
    settext(a.title,L"BOOTRIVA");
    settext(a.subtitle,tr(a,L"Pokreni sustav. Sigurno i jednostavno.",L"Boot made simple. Safety built in."));
    settext(a.version,L"v0.2.3  |  BRENDIGO");
    settext(a.lang,a.english?L"HR  /  EN ✓":L"HR ✓  /  EN");
    settext(a.catalog_label,tr(a,L"01   KATALOG SLUŽBENIH WINDOWS ISO",L"01   OFFICIAL WINDOWS ISO CATALOG"));
    settext(a.catalog_open,tr(a,L"OTVORI MICROSOFT",L"OPEN MICROSOFT"));
    settext(a.device_label,tr(a,L"02   ODABERI USB UREĐAJ",L"02   SELECT USB DEVICE"));
    settext(a.image_label,tr(a,L"03   ODABERI ISO / IMG / BIN DATOTEKU",L"03   SELECT ISO / IMG / BIN IMAGE"));
    settext(a.refresh,tr(a,L"Osvježi",L"Refresh"));
    settext(a.browse,tr(a,L"Odaberi...",L"Browse..."));
    settext(a.hash,L"SHA-256");
    settext(a.mode_label,tr(a,L"04   WINDOWS 10/11 INSTALACIJSKI USB",L"04   WINDOWS 10/11 INSTALLATION USB"));
    settext(a.winusb,tr(a,L"KREIRAJ WINDOWS USB",L"CREATE WINDOWS USB"));
    settext(a.info,tr(a,L"SIGURNOST  |  Uređaj se briše tek nakon potvrde BRISI. Provjeri broj i veličinu USB diska.",
                          L"SAFETY  |  Erasure requires typed confirmation. Verify the target USB disk number and capacity."));
    update_catalog_note(a);
    update_buttons(a);
}
void refresh_drives(App& a){
    a.drives=bootriva::win::detect_removable_usb();
    SendMessageW(a.dev,CB_RESETCONTENT,0,0);
    for(const auto& d:a.drives){
        auto str=bootriva::win::describe(d);
        SendMessageW(a.dev,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(str.c_str()));
    }
    if(!a.drives.empty())SendMessageW(a.dev,CB_SETCURSEL,0,0);
    status(a,a.drives.empty()?tr(a,L"Nema sigurnih uklonjivih USB uređaja. Provjeri USB i administratorske ovlasti.",
                                    L"No eligible removable USB devices. Check USB and administrator privileges."):
                         tr(a,L"USB uređaji pronađeni: ",L"USB devices detected: ")+std::to_wstring(a.drives.size()));
    update_buttons(a);
}
void choose_image(App& a){
    std::vector<wchar_t> filename(32768,L'\0');
    OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=a.hwnd;
    ofn.lpstrFile=filename.data();ofn.nMaxFile=static_cast<DWORD>(filename.size());
    ofn.lpstrFilter=L"Disk images (*.iso;*.img;*.bin)\0*.iso;*.img;*.bin\0All files (*.*)\0*.*\0\0";
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&ofn))return;
    a.image_path=filename.data();settext(a.image,a.image_path);
    a.image_info=bootriva::inspect_image(std::filesystem::path(a.image_path));
    if(!a.image_info.error.empty()){
        std::wstring msg(a.image_info.error.begin(),a.image_info.error.end());
        status(a,L"Image rejected: "+msg);
    } else {
        auto bytes=bootriva::human_size(a.image_info.bytes);
        std::wstring size(bytes.begin(),bytes.end());
        status(a,L"Image: "+size+(a.image_info.hybrid_writable?L"  |  MBR/GPT":L"  |  raw/optical"));
        if(a.image_info.is_iso_extension && !a.image_info.hybrid_writable)
            status(a,tr(a,L"Optički ISO: za Windows 10/11 koristi KREIRAJ WINDOWS USB. Ostali ISO sustavi još nisu podržani.",
                           L"Optical ISO: use CREATE WINDOWS USB for Windows 10/11. Other ISO formats are not yet supported."));
    }
    update_buttons(a);
}
struct ConfirmTemplate {DLGTEMPLATE dlg;WORD menu;WORD clazz;WCHAR title[32];};
INT_PTR CALLBACK confirm_proc(HWND dlg,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
    case WM_INITDIALOG:{
        bool english=l!=0;SetWindowLongPtrW(dlg,GWLP_USERDATA,english?1:0);
        HWND label=CreateWindowW(L"STATIC",english?L"All USB data will be erased. Type ERASE to proceed:":
                                              L"Svi USB podaci bit će obrisani. Upiši BRISI:",
                 WS_CHILD|WS_VISIBLE,13,13,283,38,dlg,nullptr,GetModuleHandleW(nullptr),nullptr);
        (void)label;
        HWND input=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
                         14,50,269,24,dlg,reinterpret_cast<HMENU>(201),GetModuleHandleW(nullptr),nullptr);
        CreateWindowW(L"BUTTON",english?L"Erase and write":L"Obriši i zapiši",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
                      55,89,124,25,dlg,reinterpret_cast<HMENU>(IDOK),GetModuleHandleW(nullptr),nullptr);
        CreateWindowW(L"BUTTON",english?L"Cancel":L"Odustani",WS_CHILD|WS_VISIBLE|WS_TABSTOP,
                      185,89,98,25,dlg,reinterpret_cast<HMENU>(IDCANCEL),GetModuleHandleW(nullptr),nullptr);
        SetFocus(input);return FALSE;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){EndDialog(dlg,0);return TRUE;}
        if(LOWORD(w)==IDOK){
            wchar_t typed[32]{};GetDlgItemTextW(dlg,201,typed,32);
            bool english=GetWindowLongPtrW(dlg,GWLP_USERDATA)!=0;
            if(std::wstring(typed)==(english?L"ERASE":L"BRISI")){EndDialog(dlg,1);return TRUE;}
            MessageBoxW(dlg,english?L"Type ERASE exactly.":L"Upiši točno BRISI.",L"BOOTRIVA",MB_OK|MB_ICONWARNING);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
bool confirm_write(App& a){
    ConfirmTemplate t{};
    t.dlg.style=DS_MODALFRAME|DS_CENTER|WS_POPUP|WS_CAPTION|WS_SYSMENU;
    t.dlg.cdit=0;t.dlg.x=0;t.dlg.y=0;t.dlg.cx=302;t.dlg.cy=129;
    lstrcpyW(t.title,L"BOOTRIVA  |  USB ERASE");
    return DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&t.dlg,a.hwnd,confirm_proc,a.english?1:0)==1;
}
void launch(App& a,bool do_flash){
    if(a.busy)return;
    if(a.image_path.empty()||!a.image_info.error.empty()){status(a,L"Select a valid disk image first.");return;}
    int idx=static_cast<int>(SendMessageW(a.dev,CB_GETCURSEL,0,0));
    if(do_flash && (idx<0||static_cast<std::size_t>(idx)>=a.drives.size())){status(a,L"Select an eligible USB disk first.");return;}
    Drive selected{};
    if(do_flash){
        selected=a.drives[static_cast<std::size_t>(idx)];
        bool unknown=false;
        bool same=bootriva::win::source_on_disk(a.image_path,selected.disk_number,unknown);
        SafetyInput safety{selected.usb_bus,selected.removable,false,same,!unknown,selected.capacity,a.image_info};
        auto decision=bootriva::assess(safety);
        if(!decision.allowed){status(a,std::wstring(decision.reason.begin(),decision.reason.end()));return;}
        if(!confirm_write(a))return;
    }
    a.busy=true;a.cancel=false;a.close_after=false;
    SendMessageW(a.progress,PBM_SETPOS,0,0);
    update_buttons(a);
    // Cancellation remains clickable even while the rest of the UI is locked.
    EnableWindow(a.flash,TRUE);
    if(do_flash)status(a,tr(a,L"USB zapisivanje je pokrenuto...",L"USB writing started..."));
    else status(a,L"SHA-256 started...");
    const auto file=a.image_path;const auto info=a.image_info;const auto hwnd=a.hwnd;
    a.worker=std::thread([&a,do_flash,file,info,selected,hwnd]{
        auto notify=[hwnd](unsigned percent,const std::wstring& text){
            auto* event=new ProgressEvent{percent,text};
            if(!PostMessageW(hwnd,MSG_PROGRESS,0,reinterpret_cast<LPARAM>(event)))delete event;
        };
        auto result=do_flash?bootriva::win::flash_and_verify(selected,file,info,a.cancel,notify):
                             bootriva::win::sha256(file,a.cancel,notify);
        auto* done=new DoneEvent{result.success,result.message};
        if(!PostMessageW(hwnd,MSG_DONE,0,reinterpret_cast<LPARAM>(done)))delete done;
    });
}

std::wstring ps_quote(const std::wstring& value){
    std::wstring quoted=L"'";
    for(wchar_t c:value){if(c==L'\'')quoted+=L"''";else quoted.push_back(c);}
    return quoted+L"'";
}
// PowerShell -EncodedCommand accepts Base64 of UTF-16LE. Embedding the assistant
// avoids unsigned sidecar scripts and a privileged temporary-file race.
std::string encoded_command(const std::wstring& command){
    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const auto* b=reinterpret_cast<const unsigned char*>(command.data());
    const std::size_t len=command.size()*sizeof(wchar_t);
    std::string result;result.reserve(4*((len+2)/3));
    for(std::size_t i=0;i<len;i+=3){
        const unsigned a=b[i],v=i+1<len?b[i+1]:0,c=i+2<len?b[i+2]:0;
        result.push_back(alphabet[a>>2]);
        result.push_back(alphabet[((a&3)<<4)|(v>>4)]);
        result.push_back(i+1<len?alphabet[((v&15)<<2)|(c>>6)]:'=');
        result.push_back(i+2<len?alphabet[c&63]:'=');
    }
    return result;
}
void launch_windows_installer(App& a){
    if(a.busy || a.image_path.empty() || !a.image_info.error.empty() || !a.image_info.is_iso_extension)
        return;
    int index=static_cast<int>(SendMessageW(a.dev,CB_GETCURSEL,0,0));
    if(index<0 || static_cast<std::size_t>(index)>=a.drives.size()){
        status(a,tr(a,L"Odaberi USB uređaj.",L"Select a USB device."));return;
    }
    Drive selected=a.drives[static_cast<std::size_t>(index)];
    bool unknown=false;
    bool same=bootriva::win::source_on_disk(a.image_path,selected.disk_number,unknown);
    if(unknown || same){
        status(a,L"Cannot safely verify where the ISO file is stored. Aborting.");return;
    }
    if(selected.capacity<8ULL*1024ULL*1024ULL*1024ULL){
        status(a,L"Windows installation requires an 8 GiB or larger USB disk.");return;
    }
    if(!confirm_write(a))return;
    HRSRC resource=FindResourceW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(201),RT_RCDATA);
    if(!resource){status(a,L"Embedded Windows ISO assistant is missing.");return;}
    HGLOBAL loaded=LoadResource(GetModuleHandleW(nullptr),resource);
    DWORD size=SizeofResource(GetModuleHandleW(nullptr),resource);
    const char* bytes=loaded?static_cast<const char*>(LockResource(loaded)):nullptr;
    if(!bytes || !size){status(a,L"Windows ISO assistant could not be loaded.");return;}
    int chars=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes,static_cast<int>(size),nullptr,0);
    if(chars<=0){status(a,L"Windows ISO assistant has invalid encoding.");return;}
    std::wstring script(static_cast<std::size_t>(chars),L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes,static_cast<int>(size),script.data(),chars)!=chars){
        status(a,L"Could not decode embedded Windows ISO assistant.");return;
    }
    int modeIndex=static_cast<int>(SendMessageW(a.mode,CB_GETCURSEL,0,0));
    std::wstring firmware=modeIndex==0?L"UEFI":L"BIOS";
    std::wstring filesystem=modeIndex==2?L"NTFS":L"FAT32";
    // The ISO is untrusted. Treat its path as literal PowerShell data, not code.
    std::wstring wrapper=L"$ErrorActionPreference='Stop'\ntry {\n& ([ScriptBlock]::Create(@'\n"+
        script+L"\n'@)) -IsoPath "+ps_quote(a.image_path)+L" -DiskNumber "+
        std::to_wstring(selected.disk_number)+L" -Firmware "+ps_quote(firmware)+
        L" -Filesystem "+ps_quote(filesystem)+L" -Execute -ConfirmErase ERASE\n"+
        L"} catch { Write-Host $_ -ForegroundColor Red; exit 1 }\n";
    std::string encoded=encoded_command(wrapper);
    if(encoded.size()>26000){status(a,L"Embedded assistant exceeds safe Windows command-line limit.");return;}
    wchar_t systemDir[MAX_PATH]{};
    if(!GetSystemDirectoryW(systemDir,MAX_PATH)){status(a,L"Windows system directory not found.");return;}
    std::wstring executable=std::wstring(systemDir)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    if(GetFileAttributesW(executable.c_str())==INVALID_FILE_ATTRIBUTES){
        status(a,L"Windows PowerShell 5.1 was not found.");return;
    }
    std::wstring args=L"\""+executable+L"\" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand "+
        std::wstring(encoded.begin(),encoded.end());
    std::vector<wchar_t> command(args.begin(),args.end());command.push_back(L'\0');
    STARTUPINFOW startup{};startup.cb=sizeof(startup);
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,
                       CREATE_NEW_CONSOLE,nullptr,nullptr,&startup,&process)){
        status(a,L"Could not launch Windows ISO assistant ("+std::to_wstring(GetLastError())+L").");return;
    }
    CloseHandle(process.hThread);
    a.busy=true;a.external_process=true;a.cancel=false;a.close_after=false;
    update_buttons(a);
    status(a,tr(a,L"Windows USB alat radi u konzoli. Ne uklanjaj USB dok rad ne završi.",
                  L"Windows USB assistant is running in a console. Do not remove the USB drive."));
    HWND hwnd=a.hwnd;HANDLE handle=process.hProcess;
    a.worker=std::thread([hwnd,handle]{
        DWORD exitCode=1;
        DWORD wait=WaitForSingleObject(handle,INFINITE);
        bool completed=wait==WAIT_OBJECT_0 && GetExitCodeProcess(handle,&exitCode)!=0 && exitCode==0;
        CloseHandle(handle);
        auto* done=new DoneEvent{completed,completed?
            L"Windows media assistant finished; physical boot verification still required.":
            L"Windows media assistant failed or was interrupted. Check USB contents before use."};
        if(!PostMessageW(hwnd,MSG_DONE,0,reinterpret_cast<LPARAM>(done)))delete done;
    });
}
void paint_button(DRAWITEMSTRUCT* item){
    const bool disabled=(item->itemState&ODS_DISABLED)!=0;
    const bool pressed=(item->itemState&ODS_SELECTED)!=0;
    const bool primary=item->CtlID==ID_FLASH || item->CtlID==ID_WINUSB;
    const bool source=item->CtlID==ID_CATALOG_OPEN;
    COLORREF bg=disabled?RGB(55,67,81):(primary?(pressed?RGB(16,159,171):ACCENT):(source?RGB(26,86,109):RGB(37,52,72)));
    COLORREF fg=disabled?MUTED:(primary?RGB(8,29,36):FG);
    HBRUSH brush=CreateSolidBrush(bg);
    HPEN pen=CreatePen(PS_SOLID,1,disabled?RGB(64,77,91):(primary?ACCENT:RGB(63,87,105)));
    HGDIOBJ oldBrush=SelectObject(item->hDC,brush),oldPen=SelectObject(item->hDC,pen);
    RECT button=item->rcItem;
    InflateRect(&button,-1,-1);
    RoundRect(item->hDC,button.left,button.top,button.right,button.bottom,12,12);
    SelectObject(item->hDC,oldPen);SelectObject(item->hDC,oldBrush);
    DeleteObject(pen);DeleteObject(brush);
    wchar_t title[128]{};GetWindowTextW(item->hwndItem,title,128);
    SetTextColor(item->hDC,fg);SetBkMode(item->hDC,TRANSPARENT);
    SelectObject(item->hDC,reinterpret_cast<HFONT>(SendMessageW(item->hwndItem,WM_GETFONT,0,0)));
    RECT r=item->rcItem;DrawTextW(item->hDC,title,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if(item->itemState&ODS_FOCUS){RECT inset=item->rcItem;InflateRect(&inset,-3,-3);DrawFocusRect(item->hDC,&inset);}
}
bool create_controls(App& a){
    a.background=CreateSolidBrush(BG);a.panel=CreateSolidBrush(PANEL);
    a.font=CreateFontW(-scale(a,16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    a.title_font=CreateFontW(-scale(a,28),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    a.small_font=CreateFontW(-scale(a,13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    a.title=child(a.hwnd,L"STATIC",L"BOOTRIVA",SS_LEFT,ID_TITLE);
    a.subtitle=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_SUBTITLE);
    a.version=child(a.hwnd,L"STATIC",L"",SS_RIGHT,ID_VERSION);
    a.lang=child(a.hwnd,L"BUTTON",L"HR / EN",BS_OWNERDRAW|WS_TABSTOP,ID_LANG);
    a.catalog_label=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_CATALOG_LABEL);
    a.catalog=child(a.hwnd,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,ID_CATALOG);
    a.catalog_open=child(a.hwnd,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,ID_CATALOG_OPEN);
    a.catalog_note=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_CATALOG_NOTE);
    if(a.catalog){
        for(const auto& item:official_catalog)
            SendMessageW(a.catalog,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(item.title));
        SendMessageW(a.catalog,CB_SETCURSEL,0,0);
    }
    a.device_label=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_L_DEVICE);
    a.dev=child(a.hwnd,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,ID_DEV);
    a.refresh=child(a.hwnd,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,ID_REFRESH);
    a.image_label=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_L_IMAGE);
    a.image=child(a.hwnd,L"EDIT",L"",ES_READONLY|ES_AUTOHSCROLL|WS_BORDER,ID_IMAGE);
    a.browse=child(a.hwnd,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,ID_BROWSE);
    a.hash=child(a.hwnd,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,ID_HASH);
    a.flash=child(a.hwnd,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,ID_FLASH);
    a.mode_label=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_MODE_LABEL);
    a.mode=child(a.hwnd,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,ID_MODE);
    a.winusb=child(a.hwnd,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,ID_WINUSB);
    if(a.mode){
        for(const wchar_t* value:{L"UEFI  /  FAT32",L"Legacy BIOS  /  FAT32",L"Legacy BIOS  /  NTFS"})
            SendMessageW(a.mode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));
        SendMessageW(a.mode,CB_SETCURSEL,0,0);
    }
    a.info=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_INFO);
    a.progress=child(a.hwnd,PROGRESS_CLASSW,L"",PBS_SMOOTH,ID_PROGRESS);
    a.status=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_STATUS);
    a.log=child(a.hwnd,L"EDIT",L"",ES_READONLY|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|WS_BORDER,ID_LOG);
    // Win32 CreateWindowEx can fail. Check every required child before dereferencing it.
    for(HWND h:{a.title,a.subtitle,a.version,a.lang,a.catalog_label,a.catalog,a.catalog_open,a.catalog_note,a.device_label,a.dev,a.refresh,
                a.image_label,a.image,a.browse,a.hash,a.flash,a.mode_label,a.mode,a.winusb,
                a.info,a.progress,a.status,a.log}){
        if(!h)return false;
    }
    for(HWND h:{a.subtitle,a.version,a.catalog_label,a.catalog,a.catalog_open,a.catalog_note,a.device_label,a.dev,a.refresh,a.image_label,a.image,a.browse,a.hash,a.flash,a.mode_label,a.mode,a.winusb,a.info,a.status,a.log,a.lang})font(h,a.font);
    font(a.title,a.title_font);font(a.version,a.small_font);font(a.info,a.small_font);font(a.log,a.small_font);font(a.catalog_note,a.small_font);
    translate(a);
    RECT r;GetClientRect(a.hwnd,&r);layout(a,r.right-r.left,r.bottom-r.top);
    refresh_drives(a);
    return true;
}
void refresh_fonts(App& a){
    HFONT previous=a.font,previousTitle=a.title_font,previousSmall=a.small_font;
    a.font=CreateFontW(-scale(a,16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    a.title_font=CreateFontW(-scale(a,28),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    a.small_font=CreateFontW(-scale(a,13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    for(HWND control:{a.subtitle,a.lang,a.catalog_label,a.catalog,a.catalog_open,
            a.device_label,a.dev,a.refresh,a.image_label,a.image,a.browse,a.hash,
            a.flash,a.mode_label,a.mode,a.winusb,a.status})if(control)font(control,a.font);
    for(HWND control:{a.version,a.info,a.log,a.catalog_note})if(control)font(control,a.small_font);
    if(a.title)font(a.title,a.title_font);
    if(previous)DeleteObject(previous);
    if(previousTitle)DeleteObject(previousTitle);
    if(previousSmall)DeleteObject(previousSmall);
}
LRESULT CALLBACK wndproc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){
        auto* cs=reinterpret_cast<CREATESTRUCTW*>(l);
        if(!cs || !cs->lpCreateParams)return FALSE;
        auto* owner=static_cast<App*>(cs->lpCreateParams);
        // WM_CREATE is dispatched inside CreateWindowEx, *before* that call
        // returns to wWinMain. All child creation must already know the HWND.
        owner->hwnd=hwnd;
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(owner));
        return TRUE;
    }
    if(!app)return DefWindowProcW(hwnd,message,w,l);
    App& a=*app;
    switch(message){
    case WM_CREATE:{
        BOOL dark=TRUE;
        DwmSetWindowAttribute(hwnd,20,&dark,sizeof(dark));
        // Windows 11 uses rounded corners and a seamless dark caption.
        // Unsupported DWM attributes safely fail on older Windows versions.
        DWORD corner=2;
        DwmSetWindowAttribute(hwnd,33,&corner,sizeof(corner));
        COLORREF caption=BG;
        DwmSetWindowAttribute(hwnd,35,&caption,sizeof(caption));
        COLORREF captionText=FG;
        DwmSetWindowAttribute(hwnd,36,&captionText,sizeof(captionText));
        if(!create_controls(a)){
#if !defined(BOOTRIVA_UI_SMOKE)
            MessageBoxW(hwnd,L"Could not initialize BOOTRIVA controls. Please report this error.",L"BOOTRIVA",MB_OK|MB_ICONERROR);
#endif
            return -1;
        }
        return 0;
    }
    case WM_GETMINMAXINFO:{
        auto* m=reinterpret_cast<MINMAXINFO*>(l);
        m->ptMinTrackSize={scale(a,390),scale(a,450)};
        return 0;
    }
    case WM_SIZE:layout(a,LOWORD(l),HIWORD(l));return 0;
    case WM_VSCROLL:{
        if(reinterpret_cast<HWND>(l))break;
        SCROLLINFO current{sizeof(current)};
        current.fMask=SIF_ALL;
        GetScrollInfo(hwnd,SB_VERT,&current);
        int next=a.scroll_y;
        switch(LOWORD(w)){
        case SB_LINEUP:next-=scale(a,40);break;
        case SB_LINEDOWN:next+=scale(a,40);break;
        case SB_PAGEUP:next-=static_cast<int>(current.nPage);break;
        case SB_PAGEDOWN:next+=static_cast<int>(current.nPage);break;
        case SB_THUMBTRACK:case SB_THUMBPOSITION:next=current.nTrackPos;break;
        case SB_TOP:next=0;break;
        case SB_BOTTOM:next=scale(a,a.content_height);break;
        default:return 0;
        }
        scroll_to(a,next);
        return 0;
    }
    case WM_MOUSEWHEEL:{
        const int delta=GET_WHEEL_DELTA_WPARAM(w);
        scroll_to(a,a.scroll_y-MulDiv(delta,scale(a,75),WHEEL_DELTA));
        return 0;
    }
    case WM_DPICHANGED:{
        const auto* bounds=reinterpret_cast<const RECT*>(l);
        if(bounds)SetWindowPos(hwnd,nullptr,bounds->left,bounds->top,
                               bounds->right-bounds->left,bounds->bottom-bounds->top,
                               SWP_NOZORDER|SWP_NOACTIVATE);
        refresh_fonts(a);
        RECT client{};GetClientRect(hwnd,&client);
        layout(a,client.right,client.bottom);
        InvalidateRect(hwnd,nullptr,TRUE);
        return 0;
    }
    case WM_COMMAND:
        switch(LOWORD(w)){
        case ID_REFRESH:if(!a.busy)refresh_drives(a);break;
        case ID_BROWSE:if(!a.busy)choose_image(a);break;
        case ID_LANG:if(!a.busy){a.english=!a.english;translate(a);}break;
        case ID_HASH:if(!a.busy)launch(a,false);break;
        case ID_WINUSB:if(!a.busy)launch_windows_installer(a);break;
        case ID_CATALOG_OPEN:if(!a.busy)open_official_iso_catalog(a);break;
        case ID_CATALOG:if(HIWORD(w)==CBN_SELCHANGE)update_catalog_note(a);break;
        case ID_FLASH:if(a.busy){a.cancel=true;status(a,tr(a,L"Zatraženo zaustavljanje...",L"Cancellation requested..."));}
                      else launch(a,true);break;
        default:break;
        }
        return 0;
    case MSG_PROGRESS:{
        std::unique_ptr<ProgressEvent> evt(reinterpret_cast<ProgressEvent*>(l));
        SendMessageW(a.progress,PBM_SETPOS,evt->value,0);
        settext(a.status,evt->text+L"  "+std::to_wstring(evt->value)+L"%");
        return 0;
    }
    case MSG_DONE:{
        std::unique_ptr<DoneEvent> evt(reinterpret_cast<DoneEvent*>(l));
        if(a.worker.joinable())a.worker.join();
        a.busy=false;a.external_process=false;update_buttons(a);
        status(a,(evt->ok?L"OK  |  ":L"ERROR  |  ")+evt->text);
        if(a.close_after)DestroyWindow(hwnd);
        return 0;
    }
    case WM_DRAWITEM:paint_button(reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{
        HDC hdc=reinterpret_cast<HDC>(w);SetTextColor(hdc,FG);SetBkColor(hdc,PANEL);
        HWND control=reinterpret_cast<HWND>(l);
        if(control==a.title){SetTextColor(hdc,ACCENT);SetBkColor(hdc,BG);return reinterpret_cast<LRESULT>(a.background);}
        if(control==a.subtitle||control==a.version){
            SetTextColor(hdc,MUTED);
            SetBkColor(hdc,BG);return reinterpret_cast<LRESULT>(a.background);
        }
        SetTextColor(hdc,(control==a.catalog_note||control==a.info)?MUTED:FG);
        return reinterpret_cast<LRESULT>(a.panel);
    }
    case WM_ERASEBKGND:return 1; // One background paint; avoid a white startup flash.
    case WM_PAINT:{
        PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);
        RECT viewport{};GetClientRect(hwnd,&viewport);
        HDC buffer=CreateCompatibleDC(dc);
        HBITMAP pixels=CreateCompatibleBitmap(dc,std::max(1,static_cast<int>(viewport.right)),
                                              std::max(1,static_cast<int>(viewport.bottom)));
        if(!buffer || !pixels){
            FillRect(dc,&viewport,a.background);
            if(pixels)DeleteObject(pixels);
            if(buffer)DeleteDC(buffer);
            EndPaint(hwnd,&ps);
            return 0;
        }
        HGDIOBJ oldBitmap=SelectObject(buffer,pixels);
        FillRect(buffer,&viewport,a.background);
        const int dpi=static_cast<int>(GetDpiForWindow(hwnd));
        auto s=[dpi](int n){return MulDiv(n,dpi,96);};
        const int logicalWidth=MulDiv(viewport.right,96,dpi);
        HPEN border=CreatePen(PS_SOLID,std::max(1,s(1)),RGB(50,68,88));
        HBRUSH surface=CreateSolidBrush(PANEL);
        HGDIOBJ oldPen=SelectObject(buffer,border),oldBrush=SelectObject(buffer,surface);
        for(const auto& card:a.card_ranges){
            int top=s(card.first)-a.scroll_y;
            int bottom=s(card.second)-a.scroll_y;
            if(bottom<0 || top>viewport.bottom)continue;
            RoundRect(buffer,s(16),top,s(std::max(20,logicalWidth-16)),bottom,s(14),s(14));
        }
        SelectObject(buffer,oldPen);SelectObject(buffer,oldBrush);
        DeleteObject(border);DeleteObject(surface);
        BitBlt(dc,0,0,viewport.right,viewport.bottom,buffer,0,0,SRCCOPY);
        SelectObject(buffer,oldBitmap);DeleteObject(pixels);DeleteDC(buffer);
        EndPaint(hwnd,&ps);
        return 0;
    }
    case WM_CLOSE:
        if(a.busy){
            a.close_after=true;
            if(a.external_process)
                status(a,tr(a,L"Windows alat još radi. Prozor se zatvara po završetku; ne uklanjaj USB.",
                              L"Windows assistant is running. Window will close when it finishes; do not unplug USB."));
            else {
                a.cancel=true;
                status(a,tr(a,L"Zaustavljanje operacije prije zatvaranja...",L"Cancelling operation before closing..."));
            }
            return 0;
        }
        DestroyWindow(hwnd);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,message,w,l);
}
}
int run_app(HINSTANCE instance,int show,bool smoke){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_PROGRESS_CLASS};InitCommonControlsEx(&common);
    WNDCLASSEXW klass{sizeof(klass)};
    klass.hInstance=instance;klass.lpfnWndProc=wndproc;klass.lpszClassName=L"BootrivaMainWindow";
    klass.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));
    klass.hIconSm=klass.hIcon;
    klass.hCursor=LoadCursorW(nullptr,IDC_ARROW);klass.hbrBackground=nullptr;
    if(!RegisterClassExW(&klass))return 1;
    App app;
    // Default to a compact Rufus-like *normal window*, centered in the
    // working area (never maximized), while respecting taskbars and DPI.
    POINT screenPoint{};
    MONITORINFO mi{sizeof(mi)};
    HMONITOR monitor=MonitorFromPoint(screenPoint,MONITOR_DEFAULTTOPRIMARY);
    if(!GetMonitorInfoW(monitor,&mi))return 1;
    const RECT work=mi.rcWork;
    const int availableWidth=work.right-work.left;
    const int availableHeight=work.bottom-work.top;
    const int dpi=static_cast<int>(GetDpiForSystem());
    const int desiredWidth=MulDiv(660,dpi,96);
    const int desiredHeight=MulDiv(720,dpi,96);
    const int initialWidth=std::max(300,std::min(desiredWidth,availableWidth-48));
    const int initialHeight=std::max(320,std::min(desiredHeight,availableHeight-48));
    const int initialX=work.left+(availableWidth-initialWidth)/2;
    const int initialY=work.top+(availableHeight-initialHeight)/2;
    app.hwnd=CreateWindowExW(0,klass.lpszClassName,L"BOOTRIVA — USB Creator",
            WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_VSCROLL,
            initialX,initialY,initialWidth,initialHeight,
            nullptr,nullptr,instance,&app);
    if(!app.hwnd)return 1;
    // Never honor SW_SHOWMAXIMIZED at startup. This utility is compact by design.
    ShowWindow(app.hwnd,show==SW_HIDE?SW_HIDE:SW_SHOWNORMAL);
    UpdateWindow(app.hwnd);
    if(smoke){
        // Test the actual Win32 initialization path, not a mock UI.
        constexpr int required[]={ID_TITLE,ID_SUBTITLE,ID_DEV,ID_REFRESH,ID_IMAGE,
            ID_BROWSE,ID_HASH,ID_FLASH,ID_MODE,ID_WINUSB,ID_CATALOG,ID_CATALOG_OPEN,ID_CATALOG_NOTE,ID_LANG,ID_PROGRESS,ID_STATUS,ID_LOG};
        bool valid=true;
        for(int id:required){
            HWND control=GetDlgItem(app.hwnd,id);
            if(!control || !IsWindowVisible(control)){
                OutputDebugStringW(L"BOOTRIVA smoke: missing or hidden child control");
                valid=false;
            }
        }
        wchar_t text[80]{};
        HWND title=GetDlgItem(app.hwnd,ID_TITLE);
        if(!title || !GetWindowTextW(title,text,80) || std::wstring(text)!=L"BOOTRIVA")
            valid=false;
        RECT bounds{};
        if(!GetClientRect(app.hwnd,&bounds) || bounds.right<300 || bounds.bottom<300)
            valid=false;
        RECT outer{};
        GetWindowRect(app.hwnd,&outer);
        if((outer.right-outer.left)>=availableWidth ||
           (outer.bottom-outer.top)>=availableHeight ||
           IsZoomed(app.hwnd))valid=false;
        HWND catalog=GetDlgItem(app.hwnd,ID_CATALOG);
        if(!catalog || SendMessageW(catalog,CB_GETCOUNT,0,0)!=static_cast<LRESULT>(std::size(official_catalog)))
            valid=false;
        HWND website=GetDlgItem(app.hwnd,ID_CATALOG_OPEN);
        if(!website || !IsWindowEnabled(website))valid=false;
        // Narrow, short windows must keep controls accessible by scrolling.
        SetWindowPos(app.hwnd,nullptr,0,0,
                     std::min(scale(app,450),availableWidth-32),
                     std::min(scale(app,490),availableHeight-32),
                     SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        SendMessageW(app.hwnd,WM_VSCROLL,MAKEWPARAM(SB_BOTTOM,0),0);
        if(app.scroll_y<=0)valid=false;
        HWND log=GetDlgItem(app.hwnd,ID_LOG);
        if(!log || !IsWindowVisible(log))valid=false;
        DestroyWindow(app.hwnd);
        return valid?0:3;
    }
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    return static_cast<int>(msg.wParam);
}
#if defined(BOOTRIVA_UI_SMOKE)
int main(){return run_app(GetModuleHandleW(nullptr),SW_SHOWNORMAL,true);}
#else
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
    return run_app(instance,show,false);
}
#endif
