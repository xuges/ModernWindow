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
        wc.lpszClassName = className();
        RegisterClassEx(&wc);
    }

    ModernWindow()
        : hwnd_(NULL)
        , dragMoveArea_{ 0 }
        , snapLayoutsArea_{ 0 }
        , resizeBorder_{ 0 }
    {
    
    }

    bool create(HINSTANCE hinst, LPCWSTR title)
    {
        hwnd_ = CreateWindowEx(0, className(), title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, hinst, nullptr);
        if (hwnd_)
        {
            SetWindowLongPtr(hwnd_, GWLP_USERDATA, (LONG_PTR)this); // bind this
            MARGINS frame = { 1,1,0,1 };
            DwmExtendFrameIntoClientArea(hwnd_, &frame); // make it frameless but has shadow
            SetWindowPos(hwnd_, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        return hwnd_ != NULL;
    }

    void show()
    {
        ShowWindow(hwnd_, SW_SHOWNORMAL);
        UpdateWindow(hwnd_);
    }
    
private:
    static LPCWSTR className()
    {
        return L"ModernWindow";
    }

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto self = (ModernWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (self && self->hwnd_ == hwnd)
            return self->thisWindowProc(msg, wParam, lParam); // dispatch this
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT thisWindowProc(UINT msg, WPARAM wp, LPARAM lp)
    {
        constexpr int SnapAreaWidth = 100;
        constexpr int SnapAreaHeight = 50;
        constexpr int DragMoveAreaHeight = 50;
        constexpr int ResizeBorderWidth = 10;

        switch (msg)
        {
        case WM_SIZE:
        {
            RECT window;
            GetWindowRect(hwnd_, &window);
            int width = window.right - window.left;
            int height = window.bottom - window.top;

            dragMoveArea_.left = 0;
            dragMoveArea_.right = width - SnapAreaWidth;
            dragMoveArea_.top = 0;
            dragMoveArea_.bottom = DragMoveAreaHeight;

            snapLayoutsArea_.left = dragMoveArea_.right;
            snapLayoutsArea_.right = snapLayoutsArea_.left + SnapAreaWidth;
            snapLayoutsArea_.top = 0;
            snapLayoutsArea_.bottom = SnapAreaHeight;

            resizeBorder_ = { ResizeBorderWidth, ResizeBorderWidth, ResizeBorderWidth,ResizeBorderWidth };
            return 0;
        }
        case WM_NCCALCSIZE:
        {
            // fix maximize size
            if (wp && IsMaximized(hwnd_))
            {
                HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONULL);
                if (monitor)
                {
                    MONITORINFO info = { 0 };
                    info.cbSize = sizeof(MONITORINFO);
                    if (GetMonitorInfo(monitor, &info))
                    {
                        NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lp;
                        params->rgrc[0] = info.rcWork;
                    }
                }
            }
            return 0;
        }
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONUP:
        {
            if (wp == HTMAXBUTTON)
            {
                // translate to normal mouse button event
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

            if (PtInRect(&dragMoveArea_, pt))
                return HTCAPTION;

            if (PtInRect(&snapLayoutsArea_, pt))
                return HTMAXBUTTON;

            return HTCLIENT;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd_, &ps);

            HPEN pen = CreatePen(PS_SOLID, 1, RGB(188, 188, 188));
            HGDIOBJ oldPen = SelectObject(hdc, pen);

            Rectangle(hdc, dragMoveArea_.left, dragMoveArea_.top, dragMoveArea_.right, dragMoveArea_.bottom);
            Rectangle(hdc, snapLayoutsArea_.left, snapLayoutsArea_.top, snapLayoutsArea_.right, snapLayoutsArea_.bottom);

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

private:
    HWND hwnd_;
    RECT dragMoveArea_;
    RECT snapLayoutsArea_;
    MARGINS resizeBorder_;
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
