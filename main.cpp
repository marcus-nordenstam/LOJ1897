#include "NoesisRenderPath.h"
#include "WickedEngine.h"
#include <NsGui/InputEnums.h>
#include <Windows.h>
#include <fstream>
#include <gdiplus.h>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

#include "resource.h"

wi::Application application;
NoesisRenderPath *g_noesisRenderPath = nullptr; // Global reference for input handling

// Forward declaration
Noesis::Key ConvertWin32KeyToNoesis(int vk);

// Helper function to get executable directory and construct full path
std::wstring GetLogoPath(HINSTANCE hInstance) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(hInstance, exePath, MAX_PATH);

    // Extract directory
    std::wstring path = exePath;
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        path = path.substr(0, lastSlash + 1);
    }
    path += L"Logo.png";
    return path;
}

// Function to create and show splash screen
HWND CreateSplashScreen(HINSTANCE hInstance) {
#ifdef DEBUG
    return 0; // splash-screens are only in the way in debug mode
#endif
    // Get full path to logo
    std::wstring logoPath = GetLogoPath(hInstance);

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Load the logo image
    Bitmap *bitmap = new Bitmap(logoPath.c_str());
    if (!bitmap || bitmap->GetLastStatus() != Ok) {
        delete bitmap;
        GdiplusShutdown(gdiplusToken);
        return NULL;
    }

    int splashWidth = bitmap->GetWidth();
    int splashHeight = bitmap->GetHeight();

    // Register a simple window class for splash screen
    WNDCLASSEXW wcSplash = {0};
    wcSplash.cbSize = sizeof(WNDCLASSEX);
    wcSplash.style = CS_HREDRAW | CS_VREDRAW;
    wcSplash.lpfnWndProc = DefWindowProc;
    wcSplash.hInstance = hInstance;
    wcSplash.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcSplash.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcSplash.lpszClassName = L"SplashWindowClass";
    RegisterClassExW(&wcSplash);

    // Create a borderless, topmost layered window
    HWND hSplash =
        CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW, L"SplashWindowClass",
                        NULL, WS_POPUP, (GetSystemMetrics(SM_CXSCREEN) - splashWidth) / 2,
                        (GetSystemMetrics(SM_CYSCREEN) - splashHeight) / 2, splashWidth,
                        splashHeight, NULL, NULL, hInstance, NULL);

    if (!hSplash) {
        delete bitmap;
        GdiplusShutdown(gdiplusToken);
        return NULL;
    }

    // Create device context
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    // Create 32-bit DIB for transparency
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = splashWidth;
    bmi.bmiHeader.biHeight = -splashHeight; // Negative for top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    VOID *pBits;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // Draw the bitmap to the device context using GDI+
    Graphics graphics(hdcMem);
    graphics.SetCompositingQuality(CompositingQualityHighQuality);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    graphics.DrawImage(bitmap, 0, 0, splashWidth, splashHeight);

    // Add text overlay to the splash screen
    std::wstring splashText = L"Legends of Justice: 1897 - Copyright (c) 2025";

    // Create font
    FontFamily fontFamily(L"Arial");
    Font font(&fontFamily, 14.0f, FontStyleRegular, UnitPixel);

    // Create semi-transparent white brush for text
    SolidBrush textBrush(Color(255, 255, 255, 255)); // White text
    SolidBrush shadowBrush(Color(128, 0, 0, 0));     // Semi-transparent black for shadow

    // Calculate text position (bottom of splash screen with some padding)
    RectF layoutRect(10.0f, (float)(splashHeight - 50), (float)(splashWidth - 20), 40.0f);
    StringFormat stringFormat;
    stringFormat.SetAlignment(StringAlignmentNear);
    stringFormat.SetLineAlignment(StringAlignmentFar);

    // Draw text shadow (slightly offset)
    RectF shadowRect = layoutRect;
    shadowRect.X += 1.0f;
    shadowRect.Y += 1.0f;
    graphics.DrawString(splashText.c_str(), -1, &font, shadowRect, &stringFormat, &shadowBrush);

    // Draw text
    graphics.DrawString(splashText.c_str(), -1, &font, layoutRect, &stringFormat, &textBrush);

    // Update layered window with the bitmap
    POINT ptSrc = {0, 0};
    SIZE sizeWnd = {splashWidth, splashHeight};
    POINT ptDst = {(GetSystemMetrics(SM_CXSCREEN) - splashWidth) / 2,
                   (GetSystemMetrics(SM_CYSCREEN) - splashHeight) / 2};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(hSplash, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    // Cleanup
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    delete bitmap;
    GdiplusShutdown(gdiplusToken);

    ShowWindow(hSplash, SW_SHOW);
    UpdateWindow(hSplash);

    return hSplash;
}

// Function to load icon from PNG file and create properly sized icons
void LoadIconsFromPng(HINSTANCE hInstance, const std::wstring &pngPath, HICON &hIconLarge,
                      HICON &hIconSmall) {
    hIconLarge = NULL;
    hIconSmall = NULL;

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    Bitmap *sourceBitmap = new Bitmap(pngPath.c_str());
    if (!sourceBitmap || sourceBitmap->GetLastStatus() != Ok) {
        delete sourceBitmap;
        GdiplusShutdown(gdiplusToken);
        return;
    }

    // Windows expects 32x32 for large icon and 16x16 for small icon
    int sizes[] = {32, 16};
    HICON *icons[] = {&hIconLarge, &hIconSmall};

    for (int i = 0; i < 2; i++) {
        int iconSize = sizes[i];

        // Create resized bitmap
        Bitmap *resizedBitmap = new Bitmap(iconSize, iconSize, PixelFormat32bppARGB);
        Graphics graphics(resizedBitmap);
        graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        graphics.SetSmoothingMode(SmoothingModeHighQuality);
        graphics.DrawImage(sourceBitmap, 0, 0, iconSize, iconSize);

        // Create icon from resized bitmap
        HICON hIcon = NULL;
        resizedBitmap->GetHICON(&hIcon);
        *icons[i] = hIcon;

        delete resizedBitmap;
    }

    delete sourceBitmap;
    GdiplusShutdown(gdiplusToken);
}

static bool firstTime = true;

void trackMouse(HWND hWnd) {
    if (!firstTime)
        return;
    TRACKMOUSEEVENT tme = {};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hWnd;
    TrackMouseEvent(&tme);
    firstTime = false;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {

    // Win32 window and message loop setup:
    static auto WndProc = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT {
        switch (message) {
        case WM_SIZE:
        case WM_DPICHANGED:
            if (application.is_window_active)
                application.SetWindow(hWnd);
            break;
        case WM_CHAR:
            // Forward to Noesis first if available (menu, dialogue, or caseboard mode)
            if (g_noesisRenderPath && g_noesisRenderPath->GetNoesisView() &&
                (g_noesisRenderPath->IsMenuVisible() ||
                 g_noesisRenderPath->IsDialogueModeActive() ||
                 g_noesisRenderPath->IsCaseboardModeActive())) {
                // Forward character input to Noesis for text boxes
                uint32_t character = (uint32_t)wParam;
                // Filter out control characters except tab, newline, and carriage return
                if (character >= 32 || character == 9 || character == 10 || character == 13) {
                    g_noesisRenderPath->GetNoesisView()->Char(character);
                }
            }
            // Also forward to Wicked Engine GUI
            switch (wParam) {
            case VK_BACK:
                wi::gui::TextInputField::DeleteFromInput();
                break;
            case VK_RETURN:
                break;
            default: {
                const wchar_t c = (const wchar_t)wParam;
                wi::gui::TextInputField::AddInput(c);
                break;
            }
            }
            break;
        case WM_LBUTTONDOWN:
            // Forward mouse to Noesis when menu is visible, dialogue, or caseboard mode
            if (g_noesisRenderPath && g_noesisRenderPath->GetNoesisView() &&
                (g_noesisRenderPath->IsMenuVisible() ||
                 g_noesisRenderPath->IsDialogueModeActive() ||
                 g_noesisRenderPath->IsCaseboardModeActive())) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);

                // Handle caseboard panning
                if (g_noesisRenderPath->IsCaseboardModeActive()) {
                    g_noesisRenderPath->CaseboardPanStart(x, y);
                }

                g_noesisRenderPath->GetNoesisView()->MouseButtonDown(x, y,
                                                                     Noesis::MouseButton_Left);
            }
            break;
        case WM_LBUTTONUP:
            if (g_noesisRenderPath && g_noesisRenderPath->GetNoesisView() &&
                (g_noesisRenderPath->IsMenuVisible() ||
                 g_noesisRenderPath->IsDialogueModeActive() ||
                 g_noesisRenderPath->IsCaseboardModeActive())) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);

                // Handle caseboard panning end
                if (g_noesisRenderPath->IsCaseboardModeActive()) {
                    g_noesisRenderPath->CaseboardPanEnd();
                }

                g_noesisRenderPath->GetNoesisView()->MouseButtonUp(x, y, Noesis::MouseButton_Left);
            }
            break;
        case WM_RBUTTONDOWN:
            if (g_noesisRenderPath && g_noesisRenderPath->GetNoesisView() &&
                (g_noesisRenderPath->IsMenuVisible() ||
                 g_noesisRenderPath->IsDialogueModeActive() ||
                 g_noesisRenderPath->IsCaseboardModeActive())) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                g_noesisRenderPath->GetNoesisView()->MouseButtonDown(x, y,
                                                                     Noesis::MouseButton_Right);
            }
            break;
        case WM_RBUTTONUP:
            if (g_noesisRenderPath && g_noesisRenderPath->GetNoesisView() &&
                (g_noesisRenderPath->IsMenuVisible() ||
                 g_noesisRenderPath->IsDialogueModeActive() ||
                 g_noesisRenderPath->IsCaseboardModeActive())) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                g_noesisRenderPath->GetNoesisView()->MouseButtonUp(x, y, Noesis::MouseButton_Right);
            }
            break;
        case WM_MOUSEMOVE:
            trackMouse(hWnd);
            // Forward mouse to Noesis when menu is visible, dialogue, or caseboard mode
            if (g_noesisRenderPath && g_noesisRenderPath->GetNoesisView() &&
                (g_noesisRenderPath->IsMenuVisible() ||
                 g_noesisRenderPath->IsDialogueModeActive() ||
                 g_noesisRenderPath->IsCaseboardModeActive())) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);

                // Handle caseboard panning
                if (g_noesisRenderPath->IsCaseboardModeActive()) {
                    g_noesisRenderPath->CaseboardPanMove(x, y);
                }

                g_noesisRenderPath->GetNoesisView()->MouseMove(x, y);
            }
            break;
        case WM_MOUSELEAVE:
            firstTime = true;
            break;
        case WM_MOUSEWHEEL: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            short delta = HIWORD(wParam);
            POINT pt = {x, y};
            ScreenToClient(hWnd, &pt);
            if (g_noesisRenderPath) {
                g_noesisRenderPath->MouseWheel(pt.x, pt.y, delta);
            }
        } break;
        case WM_KEYDOWN:
            g_noesisRenderPath->TryHandleShortcut(ConvertWin32KeyToNoesis((int)wParam));
            break;
        case WM_KEYUP:
            g_noesisRenderPath->GetNoesisView()->KeyUp(ConvertWin32KeyToNoesis((int)wParam));
            break;
        case WM_INPUT:
            wi::input::rawinput::ParseMessage((void *)lParam);
            break;
        case WM_KILLFOCUS:
            application.is_window_active = false;
            break;
        case WM_SETFOCUS:
            application.is_window_active = true;
            break;
        case WM_DESTROY:
            // Save config before exiting
            if (g_noesisRenderPath) {
                g_noesisRenderPath->SaveConfig();
            }
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        return 0;
    };
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize GDI+ for icon loading
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Show splash screen
    HWND hSplash = CreateSplashScreen(hInstance);

    // Process messages for splash screen to appear
    if (hSplash) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Try to load icon from resource first (embedded in executable)
    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    HICON hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON), IMAGE_ICON, 16, 16,
                                     LR_DEFAULTCOLOR);

    // Fallback: Load from PNG file if resource icon not available
    if (!hIcon || !hIconSm) {
        if (hIcon)
            DestroyIcon(hIcon);
        if (hIconSm)
            DestroyIcon(hIconSm);

        std::wstring logoPath = GetLogoPath(hInstance);
        LoadIconsFromPng(hInstance, logoPath, hIcon, hIconSm);
    }

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = hIcon;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"LOJ1897";
    wcex.hIconSm = hIconSm;
    RegisterClassExW(&wcex);

    // 2048x1152 resolution (16:9 aspect ratio)
    const int windowWidth = 2048;
    const int windowHeight = 1152;

    // Non-resizable window style (no maximize/resize box)
    DWORD windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    // Calculate window rect to account for borders/title bar
    RECT windowRect = {0, 0, windowWidth, windowHeight};
    AdjustWindowRect(&windowRect, windowStyle, FALSE);

    // Center the window on screen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowX = (screenWidth - (windowRect.right - windowRect.left)) / 2;
    int windowY = (screenHeight - (windowRect.bottom - windowRect.top)) / 2;

    HWND hWnd =
        CreateWindowW(wcex.lpszClassName, L"Legends of Justice: 1897", windowStyle, windowX,
                      windowY, windowRect.right - windowRect.left,
                      windowRect.bottom - windowRect.top, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, SW_SHOWDEFAULT);

    // Set icon for the window as well
    if (hIcon) {
        SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
    }

    // set Win32 window to engine:
    application.SetWindow(hWnd);

    // process command line string:
    wi::arguments::Parse(lpCmdLine);

    // just show some basic info:
    application.infoDisplay.active = true;
    application.infoDisplay.watermark = true;
    application.infoDisplay.resolution = true;
    application.infoDisplay.fpsinfo = true;

    // Get executable directory
    std::string exePath = wi::helper::GetExecutablePath();
    std::string exeDir = wi::helper::GetDirectoryFromPath(exePath);

    // Setup shader paths (using pre-compiled shaders only)
    // WickedEngine expects platform-specific subdirectory (hlsl6/ for DX12)
    wi::renderer::SetShaderPath(exeDir + "shaders/");
    wi::renderer::SetShaderPath(wi::renderer::GetShaderPath() + "hlsl6/");

    wi::scene::Scene &scene = wi::scene::GetScene();

    NoesisRenderPath noesisRenderPath;
    g_noesisRenderPath = &noesisRenderPath;
    noesisRenderPath.setFXAAEnabled(true);
    noesisRenderPath.SetWindowHandle(hWnd);

    application.ActivatePath(&noesisRenderPath);

    // Close splash screen after initialization
    if (hSplash) {
        DestroyWindow(hSplash);
        hSplash = NULL;
        // Shutdown GDI+ if we're done with it
        // (We'll keep it for icon loading if needed)
    }

    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            application.Run();
        }
    }

    wi::jobsystem::ShutDown(); // waits for jobs to finish before shutdown

    g_noesisRenderPath = nullptr; // Clear reference

    return (int)msg.wParam;
}

// Helper function to convert Win32 virtual key codes to Noesis keys
Noesis::Key ConvertWin32KeyToNoesis(int vk) {
    switch (vk) {
    case VK_BACK:
        return Noesis::Key_Back;
    case VK_TAB:
        return Noesis::Key_Tab;
    case VK_RETURN:
        return Noesis::Key_Return;
    case VK_ESCAPE:
        return Noesis::Key_Escape;
    case VK_SPACE:
        return Noesis::Key_Space;
    case VK_PRIOR:
        return Noesis::Key_PageUp;
    case VK_NEXT:
        return Noesis::Key_PageDown;
    case VK_END:
        return Noesis::Key_End;
    case VK_HOME:
        return Noesis::Key_Home;
    case VK_LEFT:
        return Noesis::Key_Left;
    case VK_UP:
        return Noesis::Key_Up;
    case VK_RIGHT:
        return Noesis::Key_Right;
    case VK_DOWN:
        return Noesis::Key_Down;
    case VK_INSERT:
        return Noesis::Key_Insert;
    case VK_DELETE:
        return Noesis::Key_Delete;
    case '0':
        return Noesis::Key_D0;
    case '1':
        return Noesis::Key_D1;
    case '2':
        return Noesis::Key_D2;
    case '3':
        return Noesis::Key_D3;
    case '4':
        return Noesis::Key_D4;
    case '5':
        return Noesis::Key_D5;
    case '6':
        return Noesis::Key_D6;
    case '7':
        return Noesis::Key_D7;
    case '8':
        return Noesis::Key_D8;
    case '9':
        return Noesis::Key_D9;
    case 'A':
        return Noesis::Key_A;
    case 'B':
        return Noesis::Key_B;
    case 'C':
        return Noesis::Key_C;
    case 'D':
        return Noesis::Key_D;
    case 'E':
        return Noesis::Key_E;
    case 'F':
        return Noesis::Key_F;
    case 'G':
        return Noesis::Key_G;
    case 'H':
        return Noesis::Key_H;
    case 'I':
        return Noesis::Key_I;
    case 'J':
        return Noesis::Key_J;
    case 'K':
        return Noesis::Key_K;
    case 'L':
        return Noesis::Key_L;
    case 'M':
        return Noesis::Key_M;
    case 'N':
        return Noesis::Key_N;
    case 'O':
        return Noesis::Key_O;
    case 'P':
        return Noesis::Key_P;
    case 'Q':
        return Noesis::Key_Q;
    case 'R':
        return Noesis::Key_R;
    case 'S':
        return Noesis::Key_S;
    case 'T':
        return Noesis::Key_T;
    case 'U':
        return Noesis::Key_U;
    case 'V':
        return Noesis::Key_V;
    case 'W':
        return Noesis::Key_W;
    case 'X':
        return Noesis::Key_X;
    case 'Y':
        return Noesis::Key_Y;
    case 'Z':
        return Noesis::Key_Z;
    case VK_F1:
        return Noesis::Key_F1;
    case VK_F2:
        return Noesis::Key_F2;
    case VK_F3:
        return Noesis::Key_F3;
    case VK_F4:
        return Noesis::Key_F4;
    case VK_F5:
        return Noesis::Key_F5;
    case VK_F6:
        return Noesis::Key_F6;
    case VK_F7:
        return Noesis::Key_F7;
    case VK_F8:
        return Noesis::Key_F8;
    case VK_F9:
        return Noesis::Key_F9;
    case VK_F10:
        return Noesis::Key_F10;
    case VK_F11:
        return Noesis::Key_F11;
    case VK_F12:
        return Noesis::Key_F12;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return Noesis::Key_LeftShift;
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        return Noesis::Key_LeftCtrl;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return Noesis::Key_LeftAlt;
        // Numpad keys
    case VK_NUMPAD0:
        return Noesis::Key_NumPad0;
    case VK_NUMPAD1:
        return Noesis::Key_NumPad1;
    case VK_NUMPAD2:
        return Noesis::Key_NumPad2;
    case VK_NUMPAD3:
        return Noesis::Key_NumPad3;
    case VK_NUMPAD4:
        return Noesis::Key_NumPad4;
    case VK_NUMPAD5:
        return Noesis::Key_NumPad5;
    case VK_NUMPAD6:
        return Noesis::Key_NumPad6;
    case VK_NUMPAD7:
        return Noesis::Key_NumPad7;
    case VK_NUMPAD8:
        return Noesis::Key_NumPad8;
    case VK_NUMPAD9:
        return Noesis::Key_NumPad9;
    case VK_MULTIPLY:
        return Noesis::Key_Multiply;
    case VK_ADD:
        return Noesis::Key_Add;
    case VK_SUBTRACT:
        return Noesis::Key_Subtract;
    case VK_DECIMAL:
        return Noesis::Key_Decimal;
    case VK_DIVIDE:
        return Noesis::Key_Divide;
        // OEM keys (punctuation)
    case VK_OEM_1:
        return Noesis::Key_OemSemicolon;
    case VK_OEM_PLUS:
        return Noesis::Key_OemPlus;
    case VK_OEM_COMMA:
        return Noesis::Key_OemComma;
    case VK_OEM_MINUS:
        return Noesis::Key_OemMinus;
    case VK_OEM_PERIOD:
        return Noesis::Key_OemPeriod;
    case VK_OEM_2:
        return Noesis::Key_OemQuestion;
    case VK_OEM_3:
        return Noesis::Key_OemTilde;
    case VK_OEM_4:
        return Noesis::Key_OemOpenBrackets;
    case VK_OEM_5:
        return Noesis::Key_OemPipe;
    case VK_OEM_6:
        return Noesis::Key_OemCloseBrackets;
    case VK_OEM_7:
        return Noesis::Key_OemQuotes;
    default:
        return Noesis::Key_None;
    }
}
