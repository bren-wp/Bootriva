#include "usb_win.hpp"
#include <Windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <atomic>
#include <algorithm>
#include <filesystem>
#include <memory>
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
constexpr UINT MSG_PROGRESS=WM_APP+11,MSG_DONE=WM_APP+12;
constexpr COLORREF BG=RGB(13,21,36),FG=RGB(235,243,250),MUTED=RGB(156,175,192),ACCENT=RGB(29,197,204),PANEL=RGB(24,36,56);
struct ProgressEvent {unsigned value;std::wstring text;};
struct DoneEvent {bool ok;std::wstring text;};
struct App {
    HWND hwnd{},dev{},refresh{},image{},browse{},hash{},flash{},lang{},progress{},status{},log{},title{},subtitle{},device_label{},image_label{},info{},version{};
    HFONT font{},title_font{},small_font{};
    HBRUSH background{},panel{};
    std::vector<Drive> drives;
    std::wstring image_path;
    ImageInfo image_info;
    std::thread worker;
    std::atomic_bool cancel{false};
    bool busy=false,close_after=false,english=false;
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
void layout(App& a,int w,int h){
    if(!a.dev)return;
    const int margin=28,right=w-margin,full=w-2*margin,button=126,gap=12;
    MoveWindow(a.title,margin,18,full-130,43,TRUE);
    MoveWindow(a.subtitle,margin,65,full-130,24,TRUE);
    MoveWindow(a.version,right-175,24,175,22,TRUE);
    MoveWindow(a.lang,right-100,69,100,30,TRUE);
    MoveWindow(a.device_label,margin,128,full,23,TRUE);
    MoveWindow(a.dev,margin,154,full-button-gap,260,TRUE);
    MoveWindow(a.refresh,right-button,153,button,33,TRUE);
    MoveWindow(a.image_label,margin,217,full,23,TRUE);
    MoveWindow(a.image,margin,244,full-button-gap,35,TRUE);
    MoveWindow(a.browse,right-button,244,button,35,TRUE);
    MoveWindow(a.hash,margin,308,170,41,TRUE);
    MoveWindow(a.flash,right-202,305,202,47,TRUE);
    MoveWindow(a.info,margin,367,full,40,TRUE);
    MoveWindow(a.progress,margin,414,full,17,TRUE);
    MoveWindow(a.status,margin,439,full,27,TRUE);
    MoveWindow(a.log,margin,477,full,std::max(65,h-500),TRUE);
}
void update_buttons(App& a){
    EnableWindow(a.refresh,!a.busy);EnableWindow(a.dev,!a.busy);EnableWindow(a.browse,!a.busy);
    EnableWindow(a.hash,!a.busy && a.image_info.error.empty() && !a.image_path.empty());
    EnableWindow(a.flash,!a.busy && a.image_info.error.empty() && !a.image_path.empty() && !a.drives.empty() && (!a.image_info.is_iso_extension || a.image_info.hybrid_writable));
    settext(a.flash,tr(a,a.busy?L"ZAUSTAVI":L"ZAPIŠI USB",a.busy?L"CANCEL":L"WRITE USB"));
    EnableWindow(a.lang,!a.busy);
}
void translate(App& a){
    settext(a.title,L"BOOTRIVA");
    settext(a.subtitle,tr(a,L"Pokreni sustav. Sigurno i jednostavno.",L"Boot made simple. Safety built in."));
    settext(a.version,L"v0.2.0  |  BRENDIGO");
    settext(a.lang,a.english?L"HR  /  EN ✓":L"HR ✓  /  EN");
    settext(a.device_label,tr(a,L"01   ODABERI USB UREĐAJ",L"01   SELECT USB DEVICE"));
    settext(a.image_label,tr(a,L"02   ODABERI ISO / IMG / BIN DATOTEKU",L"02   SELECT ISO / IMG / BIN IMAGE"));
    settext(a.refresh,tr(a,L"Osvježi",L"Refresh"));
    settext(a.browse,tr(a,L"Odaberi...",L"Browse..."));
    settext(a.hash,L"SHA-256");
    settext(a.info,tr(a,L"SIGURNOST   |   Samo uklonjivi USB diskovi. ISO mora biti hibridni/raw. Svi podaci na USB-u bit će izbrisani.",
                          L"SAFETY   |   Removable USB only. ISO must be hybrid/raw. All data on the USB will be erased."));
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
            status(a,tr(a,L"Ovaj ISO nije hibridan i trenutno se ne može zapisati.",
                           L"This optical-only ISO cannot be written in this release."));
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
void paint_button(DRAWITEMSTRUCT* item){
    const bool disabled=(item->itemState&ODS_DISABLED)!=0;
    const bool primary=item->CtlID==ID_FLASH;
    COLORREF bg=disabled?RGB(58,69,81):(primary?ACCENT:PANEL);
    COLORREF fg=disabled?MUTED:(primary?RGB(8,29,36):FG);
    HBRUSH brush=CreateSolidBrush(bg);FillRect(item->hDC,&item->rcItem,brush);DeleteObject(brush);
    wchar_t title[128]{};GetWindowTextW(item->hwndItem,title,128);
    SetTextColor(item->hDC,fg);SetBkMode(item->hDC,TRANSPARENT);
    SelectObject(item->hDC,reinterpret_cast<HFONT>(SendMessageW(item->hwndItem,WM_GETFONT,0,0)));
    RECT r=item->rcItem;DrawTextW(item->hDC,title,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if(item->itemState&ODS_FOCUS){RECT inset=item->rcItem;InflateRect(&inset,-3,-3);DrawFocusRect(item->hDC,&inset);}
}
void create_controls(App& a){
    a.background=CreateSolidBrush(BG);a.panel=CreateSolidBrush(PANEL);
    a.font=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    a.title_font=CreateFontW(-36,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    a.small_font=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    a.title=child(a.hwnd,L"STATIC",L"BOOTRIVA",SS_LEFT,ID_TITLE);
    a.subtitle=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_SUBTITLE);
    a.version=child(a.hwnd,L"STATIC",L"",SS_RIGHT,ID_VERSION);
    a.lang=child(a.hwnd,L"BUTTON",L"HR / EN",BS_OWNERDRAW|WS_TABSTOP,ID_LANG);
    a.device_label=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_L_DEVICE);
    a.dev=child(a.hwnd,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,ID_DEV);
    a.refresh=child(a.hwnd,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,ID_REFRESH);
    a.image_label=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_L_IMAGE);
    a.image=child(a.hwnd,L"EDIT",L"",ES_READONLY|ES_AUTOHSCROLL|WS_BORDER,ID_IMAGE);
    a.browse=child(a.hwnd,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,ID_BROWSE);
    a.hash=child(a.hwnd,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,ID_HASH);
    a.flash=child(a.hwnd,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,ID_FLASH);
    a.info=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_INFO);
    a.progress=child(a.hwnd,PROGRESS_CLASSW,L"",PBS_SMOOTH,ID_PROGRESS);
    a.status=child(a.hwnd,L"STATIC",L"",SS_LEFT,ID_STATUS);
    a.log=child(a.hwnd,L"EDIT",L"",ES_READONLY|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|WS_BORDER,ID_LOG);
    for(HWND h:{a.subtitle,a.version,a.device_label,a.dev,a.refresh,a.image_label,a.image,a.browse,a.hash,a.flash,a.info,a.status,a.log,a.lang})font(h,a.font);
    font(a.title,a.title_font);font(a.version,a.small_font);font(a.info,a.small_font);font(a.log,a.small_font);
    translate(a);
    RECT r;GetClientRect(a.hwnd,&r);layout(a,r.right-r.left,r.bottom-r.top);
    refresh_drives(a);
}
LRESULT CALLBACK wndproc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){
        auto* cs=reinterpret_cast<CREATESTRUCTW*>(l);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return TRUE;
    }
    if(!app)return DefWindowProcW(hwnd,message,w,l);
    App& a=*app;
    switch(message){
    case WM_CREATE:{BOOL dark=TRUE;DwmSetWindowAttribute(hwnd,20,&dark,sizeof(dark));create_controls(a);return 0;}
    case WM_GETMINMAXINFO:{
        auto* m=reinterpret_cast<MINMAXINFO*>(l);m->ptMinTrackSize={720,610};return 0;
    }
    case WM_SIZE:layout(a,LOWORD(l),HIWORD(l));return 0;
    case WM_COMMAND:
        switch(LOWORD(w)){
        case ID_REFRESH:if(!a.busy)refresh_drives(a);break;
        case ID_BROWSE:if(!a.busy)choose_image(a);break;
        case ID_LANG:if(!a.busy){a.english=!a.english;translate(a);}break;
        case ID_HASH:if(!a.busy)launch(a,false);break;
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
        a.busy=false;update_buttons(a);
        status(a,(evt->ok?L"OK  |  ":L"ERROR  |  ")+evt->text);
        if(a.close_after)DestroyWindow(hwnd);
        return 0;
    }
    case WM_DRAWITEM:paint_button(reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{
        HDC hdc=reinterpret_cast<HDC>(w);SetTextColor(hdc,FG);SetBkColor(hdc,PANEL);
        HWND control=reinterpret_cast<HWND>(l);
        if(control==a.title){SetTextColor(hdc,ACCENT);SetBkColor(hdc,BG);return reinterpret_cast<LRESULT>(a.background);}
        if(control==a.subtitle||control==a.version||control==a.device_label||control==a.image_label||control==a.info||control==a.status){
            SetTextColor(hdc,(control==a.info||control==a.version||control==a.subtitle)?MUTED:FG);
            SetBkColor(hdc,BG);return reinterpret_cast<LRESULT>(a.background);
        }
        return reinterpret_cast<LRESULT>(a.panel);
    }
    case WM_PAINT:{
        PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);
        RECT r{};GetClientRect(hwnd,&r);FillRect(dc,&r,a.background);
        RECT top{0,108,r.right,110};HBRUSH line=CreateSolidBrush(RGB(32,54,72));FillRect(dc,&top,line);DeleteObject(line);
        EndPaint(hwnd,&ps);return 0;
    }
    case WM_CLOSE:
        if(a.busy){a.close_after=true;a.cancel=true;status(a,tr(a,L"Zaustavljanje operacije prije zatvaranja...",L"Cancelling operation before closing..."));return 0;}
        DestroyWindow(hwnd);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_PROGRESS_CLASS};InitCommonControlsEx(&common);
    WNDCLASSEXW klass{sizeof(klass)};
    klass.hInstance=instance;klass.lpfnWndProc=wndproc;klass.lpszClassName=L"BootrivaMainWindow";
    klass.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));
    klass.hIconSm=klass.hIcon;
    klass.hCursor=LoadCursorW(nullptr,IDC_ARROW);klass.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
    if(!RegisterClassExW(&klass))return 1;
    App app;
    app.hwnd=CreateWindowExW(0,klass.lpszClassName,L"BOOTRIVA — USB Creator",WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,CW_USEDEFAULT,860,660,nullptr,nullptr,instance,&app);
    if(!app.hwnd)return 1;
    ShowWindow(app.hwnd,show);UpdateWindow(app.hwnd);
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    return static_cast<int>(msg.wParam);
}
