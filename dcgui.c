#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>

#define APP_NAME "DC Res Patch Depth Fix"
#define ID_PICK  100
#define ID_FIX   101
#define ID_LOG   102
#define ID_PATH  103

int dcfix_sweep(const char *dir, void (*say)(const char *));
int dcfix_already(void);

static HWND g_main, g_log, g_path, g_fix;
static char g_dir[MAX_PATH];
static HFONT g_font;

static void say(const char *s)
{
    int n = GetWindowTextLengthA(g_log);
    SendMessageA(g_log, EM_SETSEL, (WPARAM)n, (LPARAM)n);
    SendMessageA(g_log, EM_REPLACESEL, FALSE, (LPARAM)s);
}

static void sayf(const char *fmt, ...)
{
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    say(b);
}

static const GUID DC_CLSID_FileOpenDialog =
    {0xDC1C5A9C,0xE88A,0x4dde,{0xA5,0xA1,0x60,0xF8,0x2A,0x20,0xAE,0xF7}};
static const GUID DC_IID_IFileOpenDialog =
    {0xd57c7288,0xd4ad,0x4768,{0xbe,0x02,0x9d,0x96,0x95,0x32,0xd9,0x60}};

static int pick_folder(char *out, size_t n)
{
    IFileOpenDialog *dlg = NULL;
    IShellItem *item = NULL;
    DWORD opts = 0;
    int ok = 0;
    HRESULT hr;
    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
        return 0;
    hr = CoCreateInstance(&DC_CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                          &DC_IID_IFileOpenDialog, (void **)&dlg);
    if (SUCCEEDED(hr) && dlg) {
        if (SUCCEEDED(IFileOpenDialog_GetOptions(dlg, &opts)))
            IFileOpenDialog_SetOptions(dlg, opts | FOS_PICKFOLDERS |
                                       FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
                                       FOS_NOCHANGEDIR);
        IFileOpenDialog_SetTitle(dlg, L"The game patch folder");
        if (SUCCEEDED(IFileOpenDialog_Show(dlg, g_main)) &&
            SUCCEEDED(IFileOpenDialog_GetResult(dlg, &item)) && item) {
            PWSTR w = NULL;
            if (SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &w)) && w) {
                if (WideCharToMultiByte(CP_ACP, 0, w, -1, out, (int)n, NULL, NULL) > 0)
                    ok = 1;
                CoTaskMemFree(w);
            }
            IShellItem_Release(item);
        }
        IFileOpenDialog_Release(dlg);
    }
    CoUninitialize();
    return ok;
}

static void layout(void)
{
    RECT r;
    GetClientRect(g_main, &r);
    MoveWindow(GetDlgItem(g_main, ID_PICK), 10, 8, 250, 26, TRUE);
    MoveWindow(g_path, 10, 38, r.right - 20, 16, TRUE);
    MoveWindow(g_fix, 10, 58, r.right - 20, 28, TRUE);
    MoveWindow(g_log, 10, 92, r.right - 20, r.bottom - 102, TRUE);
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_SIZE: layout(); return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)l;
        mm->ptMinTrackSize.x = 460;
        mm->ptMinTrackSize.y = 240;
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w) == ID_PICK) {
            if (pick_folder(g_dir, sizeof g_dir)) {
                SetWindowTextA(g_path, g_dir);
                EnableWindow(g_fix, TRUE);
            }
            return 0;
        }
        if (LOWORD(w) == ID_FIX) {
            int n;
            SetWindowTextA(g_log, "");
            sayf("%s\r\n", g_dir);
            EnableWindow(g_fix, FALSE);
            n = dcfix_sweep(g_dir, say);
            if (n > 0)
                sayf("%d shader(s) changed. Delete the shader cache before you "
                     "play.\r\n", n);
            else if (dcfix_already())
                say("Already fixed, nothing to do.\r\n");
            else
                say("No depth pass in there. Pick the folder your game update "
                    "installed.\r\n");
            EnableWindow(g_fix, TRUE);
            return 0;
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASSA wc;
    MSG msg;
    HWND kids[4];
    int i;
    (void)prev; (void)cmd;
    InitCommonControls();
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = APP_NAME;
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(1));
    RegisterClassA(&wc);
    {
        int w = 460, hgt = 240;
        int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
        int sy = (GetSystemMetrics(SM_CYSCREEN) - hgt) / 2;
        g_main = CreateWindowA(APP_NAME, APP_NAME, WS_OVERLAPPEDWINDOW,
                               sx, sy, w, hgt, NULL, NULL, inst, NULL);
    }
    if (!g_main) return 1;
    g_font = CreateFontA(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    kids[0] = CreateWindowA("BUTTON", "Select game patch folder...", WS_CHILD | WS_VISIBLE,
                            0, 0, 0, 0, g_main, (HMENU)ID_PICK, inst, NULL);
    g_path = CreateWindowA("STATIC", "No folder picked yet", WS_CHILD | WS_VISIBLE,
                           0, 0, 0, 0, g_main, (HMENU)ID_PATH, inst, NULL);
    g_fix = CreateWindowA("BUTTON", "Apply the fix",
                          WS_CHILD | WS_VISIBLE | WS_DISABLED,
                          0, 0, 0, 0, g_main, (HMENU)ID_FIX, inst, NULL);
    g_log = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                            ES_READONLY | ES_AUTOVSCROLL,
                            0, 0, 0, 0, g_main, (HMENU)ID_LOG, inst, NULL);
    kids[1] = g_path; kids[2] = g_fix; kids[3] = g_log;
    for (i = 0; i < 4; i++) SendMessageA(kids[i], WM_SETFONT, (WPARAM)g_font, TRUE);
    layout();
    ShowWindow(g_main, show);
    say("Pick your game's patch folder and press the button.\r\n\r\n"
        "The pass that builds the half resolution depth buffer covers only 960 "
        "by 540 of it, which is right at 1080p and leaves the rest stale above "
        "that.\r\n");
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
