#include <windows.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <string>

std::wstring g_dllPath;
HWND hPathLabel;

DWORD GetProcessIdByName(const std::wstring& name) {
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
    
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (name == pe32.szExeFile) {
                CloseHandle(hSnapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return 0;
}

bool InjectDLL(DWORD pid, const std::wstring& dllPath) {
    DWORD access = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;
    HANDLE hProcess = OpenProcess(access, FALSE, pid);
    if (!hProcess) return false;

    size_t pathSize = (dllPath.length() + 1) * sizeof(wchar_t);
    void* allocMem = VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!allocMem) { CloseHandle(hProcess); return false; }

    if (!WriteProcessMemory(hProcess, allocMem, dllPath.c_str(), pathSize, nullptr)) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    LPTHREAD_START_ROUTINE loadLibAddr = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW");

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, loadLibAddr, allocMem, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    DWORD exitCode;
    GetExitCodeThread(hThread, &exitCode);
    
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);

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
        case WM_COMMAND: {
            if (LOWORD(wParam) == 1001) {
                OPENFILENAMEW ofn = {0};
                wchar_t szFile[MAX_PATH] = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"DLL Files\0*.dll\0All Files\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                
                if (GetOpenFileNameW(&ofn)) {
                    g_dllPath = szFile;
                    SetWindowTextW(hPathLabel, g_dllPath.c_str());
                }
            }
            if (LOWORD(wParam) == 1002) {
                if (g_dllPath.empty()) {
                    MessageBoxW(hwnd, L"Сначала выбери DLL, блять", L"Ошибка", MB_ICONERROR);
                    return 0;
                }
                
                DWORD pid = GetProcessIdByName(L"cs2.exe");
                if (!pid) {
                    MessageBoxW(hwnd, L"cs2.exe не найден. Запусти игру, сука", L"Ошибка", MB_ICONERROR);
                    return 0;
                }
                
                if (InjectDLL(pid, g_dllPath)) {
                    // CS2 использует SDL, ищем по классу или заголовку
                    HWND hCs2 = FindWindowW(L"SDL_app", L"Counter-Strike 2");
                    if (!hCs2) hCs2 = FindWindowW(nullptr, L"Counter-Strike 2");
                    
                    if (hCs2) {
                        ShowWindow(hCs2, SW_RESTORE);
                        SetForegroundWindow(hCs2);
                    }
                    MessageBoxW(hwnd, L"Успешно заинжекчено, нахуй", L"Успех", MB_ICONINFORMATION);
                } else {
                    MessageBoxW(hwnd, L"Инъекция проебалась", L"Ошибка", MB_ICONERROR);
                }
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CS2InjectorClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"CS2InjectorClass", L"Fox Injector", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200, nullptr, nullptr, hInst, nullptr);
    
    ShowWindow(hwnd, nCmdShow);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
