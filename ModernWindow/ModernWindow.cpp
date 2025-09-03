#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

#include <string>

using namespace std;

class ModernWindow
{
public:
    static constexpr LPCWSTR ClassName = L"ModernWindow";

    static void registerWindowClass(HINSTANCE hInst)
    {
        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.hInstance = hInst;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = windowProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOWFRAME);
        wc.lpszClassName = ClassName;
        RegisterClassEx(&wc);
    }

    ModernWindow()
        : hwnd_(NULL)
        , dragMoveArea_{ 0 }
        , snapLayoutsArea_{ 0 }
    {
    
    }

    bool create(HINSTANCE hinst, LPCWSTR title)
    {
        hwnd_ = CreateWindowEx(0, ClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, hinst, nullptr);
        if (hwnd_)
        {
            SetWindowLongPtr(hwnd_, GWLP_USERDATA, (LONG_PTR)this); // bind this
            SetWindowLongPtr(hwnd_, GWL_STYLE, WS_POPUP | WS_THICKFRAME | WS_MAXIMIZEBOX); // change style (after default pos and size or direct set pos and size)
            MARGINS margins = { 0,0,1,0 };
            DwmExtendFrameIntoClientArea(hwnd_, &margins);
            SetWindowPos(hwnd_, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        return hwnd_ != NULL;
    }

    void show()
    {
        ShowWindow(hwnd_, SW_SHOWNORMAL);
        UpdateWindow(hwnd_);
    }

    void update()
    {
        InvalidateRect(hwnd_, NULL, FALSE);
        UpdateWindow(hwnd_);
    }
    
private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto self = (ModernWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (self && self->hwnd_ == hwnd)
            return self->thisWindowProc(msg, wParam, lParam); // dispatch this
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT thisWindowProc(UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
        case WM_WINDOWPOSCHANGED:
        {
            RECT window = { 0 };
            GetClientRect(hwnd_, &window);

            int width = window.right - window.left;
            constexpr int SnapAreaWidth = 100;
            constexpr int SnapAreaHeight = 50;
            constexpr int DragMoveAreaHeight = 50;

            dragMoveArea_.left = 0;
            dragMoveArea_.right = width - SnapAreaWidth;
            dragMoveArea_.top = 0;
            dragMoveArea_.bottom = DragMoveAreaHeight;

            snapLayoutsArea_.left = dragMoveArea_.right;
            snapLayoutsArea_.right = snapLayoutsArea_.left + SnapAreaWidth;
            snapLayoutsArea_.top = 0;
            snapLayoutsArea_.bottom = SnapAreaHeight;

            break;
        }
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* info = (MINMAXINFO*)lp;
            RECT rect =
            {
                info->ptMaxPosition.x,
                info->ptMaxPosition.y,
                info->ptMaxPosition.x + info->ptMaxSize.x,
                info->ptMaxPosition.y + info->ptMaxSize.y
            };
            rect = getMonitorRectOr(rect);
            info->ptMaxPosition.x = rect.left;
            info->ptMaxPosition.y = rect.top;
            info->ptMaxSize.x = rect.right - rect.left;
            info->ptMaxSize.y = rect.bottom - rect.top;
            return 0;
        }
        case WM_NCCALCSIZE:
        {
            if (wp)
            {
                NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lp;
                if (IsMaximized(hwnd_))
                {
                    // use monitor rect for maximized
                    params->rgrc[0] = getMonitorRectOr(params->rgrc[0]);
                    return 0;
                }

                RECT origin = params->rgrc[0]; // origin is borderless
                LRESULT defRet = DefWindowProc(hwnd_, msg, wp, lp);
                params->rgrc[0].top = origin.top + 1; // restore top borderless
                return defRet;
            }
            break;
        }
        case WM_ACTIVATE:
        {
            update();
            break;
        }
        case WM_NCLBUTTONDOWN:
        {
            update();
            //fallthrough
        }
        case WM_NCLBUTTONUP:
        {
            if (wp == HTMAXBUTTON)
            {
                // translate to normal mouse button event (reject maximize button)
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ScreenToClient(hwnd_, &pt);
                SendMessage(hwnd_, msg == WM_NCLBUTTONDOWN ? WM_LBUTTONDOWN : WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
                return 0;
            }
            break;
        }
        case WM_NCHITTEST:
        {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd_, &pt);

            RECT window = { 0 };
            GetClientRect(hwnd_, &window);

            constexpr int borderWidth = 8; // TODO: dynamic get with GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER)
            constexpr int borderHeight = 8; // TODO: dynamic get with GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYPADDEDBORDER)

            LRESULT defRet = DefWindowProc(hwnd_, msg, wp, lp);
            if (defRet == HTTOPLEFT || defRet == HTTOPRIGHT)
            {
                if (window.top + borderHeight < pt.y)
                    return defRet == HTTOPLEFT ? HTLEFT : HTRIGHT; // fix corner resize
                return defRet;
            }

            if (pt.y <= window.top + borderHeight && !IsMaximized(hwnd_))
            {
                if (pt.x <= window.left + borderWidth)
                    return HTTOPLEFT; // top-left resize
                if (window.right - borderWidth <= pt.x)
                    return HTTOPRIGHT; // top-right resize
                return HTTOP; // top resize
            }

            if (PtInRect(&dragMoveArea_, pt))
                return HTCAPTION; // title bar (drag move, double-click maximize)

            if (PtInRect(&snapLayoutsArea_, pt))
                return HTMAXBUTTON; // maximize button (hover snap-layous, click rejected by WM_NCLBUTTONUP)

            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd_, &ps);

            // fill title area background
            RECT titleArea = { 0, 0, snapLayoutsArea_.right, snapLayoutsArea_.bottom };
            HBRUSH bgBrush = (HBRUSH)(COLOR_WINDOWFRAME);
            FillRect(hdc, &titleArea, bgBrush);

            HPEN pen = CreatePen(PS_SOLID, 1, RGB(188, 188, 188));
            HGDIOBJ oldPen = SelectObject(hdc, pen);

            LOGFONT font = { 0 };
            font.lfHeight = 16;
            wcscpy_s(font.lfFaceName, 16, L"Times New Roman");
            HFONT hfont = CreateFontIndirect(&font);
            HGDIOBJ oldFont = SelectObject(hdc, hfont);
            
            DrawText(hdc, L"Drag move", -1, &dragMoveArea_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DrawText(hdc, L"Snap layouts", -1, &snapLayoutsArea_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            SelectObject(hdc, oldFont);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
            DeleteObject(hfont);

            EndPaint(hwnd_, &ps);
        }
        break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        }
        return DefWindowProc(hwnd_, msg, wp, lp);
    }

    RECT getMonitorRectOr(RECT defRc)
    {
        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONULL);
        if (monitor)
        {
            MONITORINFO info = { 0 };
            info.cbSize = sizeof(MONITORINFO);
            if (GetMonitorInfo(monitor, &info))
                return info.rcWork;
        }
        return defRc;
    }

    static void debugPrint(LPCWSTR fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        WCHAR buf[512];
        int len = wvsprintf(buf, fmt, args);
        OutputDebugString(buf);
        va_end(args);
    }

    enum class WindowsVersion
    {
        Unknown,
        Windows7,
        Windows8,
        Windows8_1,
        Windows10,
        Windows11
    };

    static WindowsVersion getWindowsVersion()
    {
        static auto version = []()->WindowsVersion
            {
                int buildNumber = getWindowsBuildNumber();
                if (buildNumber >= 22000) return WindowsVersion::Windows11;  // Win11
                if (buildNumber >= 10240) return WindowsVersion::Windows10;  // Win10
                if (buildNumber >= 9600)  return WindowsVersion::Windows8_1; // Win8.1
                if (buildNumber >= 9200)  return WindowsVersion::Windows8;   // Win8
                if (buildNumber >= 7600)  return WindowsVersion::Windows7;   // Win7
                return WindowsVersion::Unknown;
            }();
        return version;
    }

    static int getWindowsBuildNumber()
    {
        HKEY hKey;
        WCHAR data[8];
        DWORD dataSize = sizeof(data);
        int buildNumber = 0;

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            if (RegQueryValueExW(hKey, L"CurrentBuildNumber", NULL, NULL, (LPBYTE)data, &dataSize) == ERROR_SUCCESS)
                buildNumber = wcstol(data, nullptr, 10);
            RegCloseKey(hKey);
        }

        return buildNumber;
    }

private:
    HWND hwnd_;
    RECT dragMoveArea_;
    RECT snapLayoutsArea_;
};

int WINAPI wWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPWSTR    lpCmdLine,
                     int       nCmdShow)
{
    ModernWindow::registerWindowClass(hInstance);
    ModernWindow window;
    window.create(hInstance, L"ModernWindow DEMO");
    window.show();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
