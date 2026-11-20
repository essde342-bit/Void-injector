#include <windows.h>
#include <windowsx.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <string>
#include <thread>
#include <atomic>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

// --- Глобальные переменные ---
std::wstring g_dllPath;
HWND hPathLabel, hMainWnd;
std::atomic<bool> g_running{true};
std::wstring g_lastMessage = L"";
std::wstring g_currentMessage = L"";

const wchar_t* GIST_URL = L"https://gist.githubusercontent.com/essde342-bit/840fd8fabcb3dcd5b49b6576f2b3ad64/raw/update.json";

// --- Сеть и парсинг ---
std::wstring DownloadString(const std::wstring& url) {
    HINTERNET hInternet = InternetOpenW(L"FoxInjector/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return L"";
    HINTERNET hUrl = InternetOpenUrlW(hInternet, url.c_str(), NULL, NULL, INTERNET_FLAG_RELOAD, 0);
    if (!hUrl) { InternetCloseHandle(hInternet); return L""; }

    char buffer[4096];
    std::string result;
    DWORD bytesRead = 0;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
        result.append(buffer, bytesRead);
    
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, result.c_str(), -1, NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, result.c_str(), -1, &wstr[0], size_needed);
    return wstr;
}

std::wstring ParseMessage(const std::wstring& json) {
    size_t start = json.find(L"\"message\":");
    if (start == std::wstring::npos) return L"";
    start = json.find(L"\"", start + 10);
    if (start == std::wstring::npos) return L"";
    size_t end = json.find(L"\"", start + 1);
    if (end == std::wstring::npos) return L"";
    return json.substr(start + 1, end - start - 1);
}

// Инициализация последнего сообщения, чтобы не спамить при старте
void InitLastMessage() {
    std::wstring json = DownloadString(GIST_URL);
    if (!json.empty()) {
        g_lastMessage = ParseMessage(json);
    }
}

// --- Кастомное окно уведомления ---
#define WM_SHOW_POPUP (WM_USER + 100)

const int POPUP_W = 420;
const int POPUP_H = 220;
const int HEADER_H = 36;

bool g_closeBtnHover = false;
RECT rcCloseBtn = {POPUP_W / 2 - 60, 155, POPUP_W / 2 + 60, 185};

void DrawPopup(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, POPUP_W, POPUP_H);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    HBRUSH bgBrush = CreateSolidBrush(RGB(245, 245, 245));
    HPEN nullPen = CreatePen(PS_NULL, 0, 0);
    HPEN oldPen = (HPEN)SelectObject(memDC, nullPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, bgBrush);
    RoundRect(memDC, 0, 0, POPUP_W, POPUP_H, 10, 10);

    HBRUSH headerBrush = CreateSolidBrush(RGB(30, 136, 170));
    SelectObject(memDC, headerBrush);
    HRGN headerRgn = CreateRoundRectRgn(0, 0, POPUP_W, HEADER_H + 5, 10, 10);
    HRGN bodyRgn = CreateRectRgn(0, 5, POPUP_W, HEADER_H + 5);
    CombineRgn(headerRgn, headerRgn, bodyRgn, RGN_OR);
    FillRgn(memDC, headerRgn, headerBrush);
    DeleteObject(headerRgn); DeleteObject(bodyRgn);

    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));
    HFONT fontHeader = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(memDC, fontHeader);
    TextOutW(memDC, 12, 11, L"SYSTEM MESSAGE", 14);

    HPEN hPenGreen = CreatePen(PS_SOLID, 1, RGB(39, 174, 96));
    HBRUSH hBrushGreen = CreateSolidBrush(RGB(46, 204, 113));
    SelectObject(memDC, hPenGreen); SelectObject(memDC, hBrushGreen);
    Ellipse(memDC, POPUP_W - 90, 8, POPUP_W - 70, 28);
    DeleteObject(hPenGreen); DeleteObject(hBrushGreen);

    HPEN hPenYellow = CreatePen(PS_SOLID, 1, RGB(243, 156, 18));
    HBRUSH hBrushYellow = CreateSolidBrush(RGB(241, 196, 15));
    SelectObject(memDC, hPenYellow); SelectObject(memDC, hBrushYellow);
    Ellipse(memDC, POPUP_W - 60, 8, POPUP_W - 40, 28);
    DeleteObject(hPenYellow); DeleteObject(hBrushYellow);

    HPEN hPenRed = CreatePen(PS_SOLID, 1, RGB(192, 57, 43));
    HBRUSH hBrushRed = CreateSolidBrush(RGB(231, 76, 60));
    SelectObject(memDC, hPenRed); SelectObject(memDC, hBrushRed);
    Ellipse(memDC, POPUP_W - 30, 8, POPUP_W - 10, 28);
    DeleteObject(hPenRed); DeleteObject(hBrushRed);

    HPEN hCrossPen = CreatePen(PS_SOLID, 4, RGB(231, 76, 60));
    SelectObject(memDC, hCrossPen); 
    SelectObject(memDC, GetStockObject(NULL_BRUSH));
    Ellipse(memDC, 130, 60, 190, 120);
    MoveToEx(memDC, 145, 75, NULL); LineTo(memDC, 175, 105);
    MoveToEx(memDC, 175, 75, NULL); LineTo(memDC, 145, 105);
    DeleteObject(hCrossPen);

    SetTextColor(memDC, RGB(60, 60, 60));
    HFONT fontText = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SelectObject(memDC, fontText);
    
    RECT textRect = {200, 60, POPUP_W - 20, 130};
    DrawTextW(memDC, g_currentMessage.c_str(), -1, &textRect, DT_WORDBREAK | DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    HBRUSH btnBrush = CreateSolidBrush(g_closeBtnHover ? RGB(200, 200, 200) : RGB(220, 220, 220));
    HPEN btnPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    SelectObject(memDC, btnBrush); SelectObject(memDC, btnPen);
    RoundRect(memDC, rcCloseBtn.left, rcCloseBtn.top, rcCloseBtn.right, rcCloseBtn.bottom, 5, 5);
    
    SetTextColor(memDC, RGB(60, 60, 60));
    HFONT fontBtn = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SelectObject(memDC, fontBtn);
    RECT btnTextRect = rcCloseBtn;
    DrawTextW(memDC, L"Close", -1, &btnTextRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    BitBlt(hdc, 0, 0, POPUP_W, POPUP_H, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBrush);
    SelectObject(memDC, oldFont);
    DeleteObject(memBmp);
    DeleteObject(bgBrush);
    DeleteObject(nullPen);
    DeleteObject(headerBrush);
    DeleteObject(btnBrush);
    DeleteObject(btnPen);
    DeleteObject(fontHeader);
    DeleteObject(fontText);
    DeleteObject(fontBtn);
    DeleteDC(memDC);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK PopupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: DrawPopup(hwnd); break;
        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_LBUTTONDOWN: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (PtInRect(&rcCloseBtn, pt)) {
                DestroyWindow(hwnd);
            } else if (pt.y < HEADER_H) {
                SetCapture(hwnd);
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
            }
            break;
        }
        case WM_LBUTTONUP: ReleaseCapture(); break;
        case WM_MOUSEMOVE: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            bool hover = PtInRect(&rcCloseBtn, pt);
            if (hover != g_closeBtnHover) {
                g_closeBtnHover = hover;
                InvalidateRect(hwnd, &rcCloseBtn, TRUE);
            }
            break;
        }
        case WM_MOUSELEAVE:
            if (g_closeBtnHover) { g_closeBtnHover = false; InvalidateRect(hwnd, &rcCloseBtn, TRUE); }
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowCustomPopup() {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = PopupWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"FoxPopupClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    int x = (GetSystemMetrics(SM_CXSCREEN) - POPUP_W) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - POPUP_H) / 2;
    
    HWND hPopup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"FoxPopupClass", L"", 
                                  WS_POPUP | WS_VISIBLE, x, y, POPUP_W, POPUP_H, NULL, NULL, GetModuleHandle(NULL), NULL);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// --- Фоновый поток ---
void BackgroundChecker() {
    while (g_running) {
        std::wstring json = DownloadString(GIST_URL);
        if (!json.empty()) {
            std::wstring msg = ParseMessage(json);
            if (!msg.empty() && msg != g_lastMessage) {
                g_lastMessage = msg;
                g_currentMessage = msg;
                if (hMainWnd) PostMessage(hMainWnd, WM_SHOW_POPUP, 0, 0);
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

// --- Основной инжектор ---
DWORD GetProcessIdByName(const std::wstring& name) {
    PROCESSENTRY32W pe32; pe32.dwSize = sizeof(pe32);
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(hSnapshot, &pe32)) {
        do { if (name == pe32.szExeFile) { CloseHandle(hSnapshot); return pe32.th32ProcessID; } } 
        while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return 0;
}

bool InjectDLL(DWORD pid, const std::wstring& dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return false;

    size_t pathSize = (dllPath.length() + 1) * sizeof(wchar_t);
    void* allocMem = VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!allocMem) { CloseHandle(hProcess); return false; }

    if (!WriteProcessMemory(hProcess, allocMem, dllPath.c_str(), pathSize, nullptr)) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE); CloseHandle(hProcess); return false;
    }

    LPTHREAD_START_ROUTINE loadLibAddr = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, loadLibAddr, allocMem, 0, nullptr);
    if (!hThread) { VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE); CloseHandle(hProcess); return false; }

    WaitForSingleObject(hThread, INFINITE);
    DWORD exitCode; GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread); VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE); CloseHandle(hProcess);
    return exitCode != 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CreateWindowW(L"BUTTON", L"Выбрать DLL", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 120, 20, 160, 30, hwnd, (HMENU)1001, nullptr, nullptr);
            hPathLabel = CreateWindowW(L"STATIC", L"DLL не выбран", WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 60, 360, 20, hwnd, (HMENU)1003, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"INJECT", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_BORDER, 120, 100, 160, 50, hwnd, (HMENU)1002, nullptr, nullptr);
            break;
        }
        case WM_SHOW_POPUP: {
            std::thread(ShowCustomPopup).detach();
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == 1001) {
                OPENFILENAMEW ofn = {0}; wchar_t szFile[MAX_PATH] = {0};
                ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"DLL Files\0*.dll\0All Files\0*.*\0"; ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) { g_dllPath = szFile; SetWindowTextW(hPathLabel, g_dllPath.c_str()); }
            }
            if (LOWORD(wParam) == 1002) {
                if (g_dllPath.empty()) { MessageBoxW(hwnd, L"Сначала выбери DLL, блять", L"Ошибка", MB_ICONERROR); return 0; }
                DWORD pid = GetProcessIdByName(L"cs2.exe");
                if (!pid) { MessageBoxW(hwnd, L"cs2.exe не найден. Запусти игру, сука", L"Ошибка", MB_ICONERROR); return 0; }
                
                if (InjectDLL(pid, g_dllPath)) {
                    HWND hCs2 = FindWindowW(L"SDL_app", L"Counter-Strike 2");
                    if (!hCs2) hCs2 = FindWindowW(nullptr, L"Counter-Strike 2");
                    if (hCs2) { ShowWindow(hCs2, SW_RESTORE); SetForegroundWindow(hCs2); }
                    MessageBoxW(hwnd, L"Успешно заинжекчено, нахуй", L"Успех", MB_ICONINFORMATION);
                } else {
                    MessageBoxW(hwnd, L"Инъекция проебалась", L"Ошибка", MB_ICONERROR);
                }
            }
            break;
        }
        case WM_DESTROY:
            g_running = false;
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.lpszClassName = L"CS2InjectorClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    hMainWnd = CreateWindowW(L"CS2InjectorClass", L"Fox Injector", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200, nullptr, nullptr, hInst, nullptr);
    
    ShowWindow(hMainWnd, nCmdShow);

    // Загружаем текущее сообщение, чтобы не показывать его при первом запуске
    InitLastMessage();

    std::thread(BackgroundChecker).detach();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
