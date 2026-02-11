#include "GrymEngine.h"

#include "NoesisRenderPath.h"
#include "common/grProjectInit.h"
#include "common/gui/ImGui/imgui.h"
#include "common/gui/ImGui/imgui_impl_win32.h"

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

// ImGui rendering resources
static wi::graphics::Shader imguiVS;
static wi::graphics::Shader imguiPS;
static wi::graphics::Texture fontTexture;
static wi::graphics::Sampler imguiSampler;
static wi::graphics::InputLayout imguiInputLayout;
static wi::graphics::PipelineState imguiPSO;

struct ImGui_Impl_Data {};

static bool ImGui_Impl_CreateDeviceObjects() {
    ImGuiIO &io = ImGui::GetIO();

    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    wi::graphics::TextureDesc textureDesc;
    textureDesc.width = width;
    textureDesc.height = height;
    textureDesc.mip_levels = 1;
    textureDesc.array_size = 1;
    textureDesc.format = wi::graphics::Format::R8G8B8A8_UNORM;
    textureDesc.bind_flags = wi::graphics::BindFlag::SHADER_RESOURCE;

    wi::graphics::SubresourceData textureData;
    textureData.data_ptr = pixels;
    textureData.row_pitch = width * wi::graphics::GetFormatStride(textureDesc.format);
    textureData.slice_pitch = textureData.row_pitch * height;

    bool success =
        wi::graphics::GetDevice()->CreateTexture(&textureDesc, &textureData, &fontTexture);
    if (!success) {
        wi::backlog::post("Failed to create ImGui font texture!");
        return false;
    }

    wi::graphics::SamplerDesc samplerDesc;
    samplerDesc.address_u = wi::graphics::TextureAddressMode::WRAP;
    samplerDesc.address_v = wi::graphics::TextureAddressMode::WRAP;
    samplerDesc.address_w = wi::graphics::TextureAddressMode::WRAP;
    samplerDesc.filter = wi::graphics::Filter::MIN_MAG_MIP_LINEAR;
    wi::graphics::GetDevice()->CreateSampler(&samplerDesc, &imguiSampler);

    io.Fonts->SetTexID((ImTextureID)&fontTexture);

    imguiInputLayout.elements = {
        {"POSITION", 0, wi::graphics::Format::R32G32_FLOAT, 0,
         (uint32_t)IM_OFFSETOF(ImDrawVert, pos),
         wi::graphics::InputClassification::PER_VERTEX_DATA},
        {"TEXCOORD", 0, wi::graphics::Format::R32G32_FLOAT, 0,
         (uint32_t)IM_OFFSETOF(ImDrawVert, uv),
         wi::graphics::InputClassification::PER_VERTEX_DATA},
        {"COLOR", 0, wi::graphics::Format::R8G8B8A8_UNORM, 0,
         (uint32_t)IM_OFFSETOF(ImDrawVert, col),
         wi::graphics::InputClassification::PER_VERTEX_DATA},
    };

    wi::graphics::PipelineStateDesc desc;
    desc.vs = &imguiVS;
    desc.ps = &imguiPS;
    desc.il = &imguiInputLayout;
    desc.dss = wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEPTHREAD);
    desc.rs = wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
    desc.bs = wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);
    desc.pt = wi::graphics::PrimitiveTopology::TRIANGLELIST;
    wi::graphics::GetDevice()->CreatePipelineState(&desc, &imguiPSO);

    return true;
}

void RenderImGui(wi::graphics::CommandList cmd) {
    ImGui::Render();
    auto drawData = ImGui::GetDrawData();
    if (!drawData || drawData->TotalVtxCount == 0)
        return;

    int fb_width = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    int fb_height = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();

    const uint64_t vbSize = sizeof(ImDrawVert) * drawData->TotalVtxCount;
    const uint64_t ibSize = sizeof(ImDrawIdx) * drawData->TotalIdxCount;
    auto vertexBufferAllocation = device->AllocateGPU(vbSize, cmd);
    auto indexBufferAllocation = device->AllocateGPU(ibSize, cmd);

    ImDrawVert *vertexCPUMem = reinterpret_cast<ImDrawVert *>(vertexBufferAllocation.data);
    ImDrawIdx *indexCPUMem = reinterpret_cast<ImDrawIdx *>(indexBufferAllocation.data);
    for (int cmdListIdx = 0; cmdListIdx < drawData->CmdListsCount; cmdListIdx++) {
        const ImDrawList *drawList = drawData->CmdLists[cmdListIdx];
        memcpy(vertexCPUMem, &drawList->VtxBuffer[0],
               drawList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(indexCPUMem, &drawList->IdxBuffer[0],
               drawList->IdxBuffer.Size * sizeof(ImDrawIdx));
        vertexCPUMem += drawList->VtxBuffer.Size;
        indexCPUMem += drawList->IdxBuffer.Size;
    }

    struct ImGuiConstants {
        float mvp[4][4];
    };

    {
        const float L = drawData->DisplayPos.x;
        const float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
        const float T = drawData->DisplayPos.y;
        const float B = drawData->DisplayPos.y + drawData->DisplaySize.y;

        ImGuiConstants constants;
        float mvp[4][4] = {
            {2.0f / (R - L), 0.0f, 0.0f, 0.0f},
            {0.0f, 2.0f / (T - B), 0.0f, 0.0f},
            {0.0f, 0.0f, 0.5f, 0.0f},
            {(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f},
        };
        memcpy(&constants.mvp, mvp, sizeof(mvp));
        device->BindDynamicConstantBuffer(constants, 0, cmd);
    }

    const wi::graphics::GPUBuffer *vbs[] = {&vertexBufferAllocation.buffer};
    const uint32_t strides[] = {sizeof(ImDrawVert)};
    const uint64_t offsets[] = {vertexBufferAllocation.offset};

    device->BindVertexBuffers(vbs, 0, 1, strides, offsets, cmd);
    device->BindIndexBuffer(&indexBufferAllocation.buffer,
                            wi::graphics::IndexBufferFormat::UINT16,
                            indexBufferAllocation.offset, cmd);

    wi::graphics::Viewport viewport;
    viewport.width = (float)fb_width;
    viewport.height = (float)fb_height;
    device->BindViewports(1, &viewport, cmd);

    device->BindPipelineState(&imguiPSO, cmd);
    device->BindSampler(&imguiSampler, 0, cmd);

    ImVec2 clip_off = drawData->DisplayPos;
    ImVec2 clip_scale = drawData->FramebufferScale;

    int32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    for (uint32_t cmdListIdx = 0; cmdListIdx < (uint32_t)drawData->CmdListsCount;
         ++cmdListIdx) {
        const ImDrawList *drawList = drawData->CmdLists[cmdListIdx];
        for (uint32_t cmdIndex = 0; cmdIndex < (uint32_t)drawList->CmdBuffer.size();
             ++cmdIndex) {
            const ImDrawCmd *pcmd = &drawList->CmdBuffer[cmdIndex];
            if (pcmd->UserCallback != NULL) {
                pcmd->UserCallback(drawList, pcmd);
            } else {
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x,
                                (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x,
                                (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                wi::graphics::Rect scissor;
                scissor.left = (int32_t)clip_min.x;
                scissor.top = (int32_t)clip_min.y;
                scissor.right = (int32_t)clip_max.x;
                scissor.bottom = (int32_t)clip_max.y;
                device->BindScissorRects(1, &scissor, cmd);

                const wi::graphics::Texture *texture =
                    (const wi::graphics::Texture *)pcmd->GetTexID();
                device->BindResource(texture, 0, cmd);

                device->DrawIndexed(pcmd->ElemCount, indexOffset + pcmd->IdxOffset,
                                    vertexOffset + (int32_t)pcmd->VtxOffset, cmd);
            }
        }
        indexOffset += (uint32_t)drawList->IdxBuffer.Size;
        vertexOffset += drawList->VtxBuffer.Size;
    }
}

// Forward declarations
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                                              WPARAM wParam, LPARAM lParam);
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
        // Forward input to ImGui so the profiler window is interactive
        if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
            return true;

        // When the profiler window is visible, let ImGui consume mouse events
        bool profilerWantsInput = g_noesisRenderPath &&
                                  g_noesisRenderPath->profilerWnd.IsVisible() &&
                                  ImGui::GetIO().WantCaptureMouse;

        switch (message) {
        case WM_SIZE:
        case WM_DPICHANGED:
            if (application.is_window_active)
                application.SetWindow(hWnd);
            break;
        case WM_KEYDOWN:
            if (!ImGui::GetIO().WantCaptureKeyboard) {
                // Forward KeyDown to Noesis so it can handle backspace, arrows, etc.
                g_noesisRenderPath->GetNoesisView()->KeyDown(
                    ConvertWin32KeyToNoesis((int)wParam));
            }
            break;
        case WM_KEYUP:
            if (!ImGui::GetIO().WantCaptureKeyboard) {
                g_noesisRenderPath->GetNoesisView()->KeyUp(
                    ConvertWin32KeyToNoesis((int)wParam));
            }
            break;
        case WM_CHAR:
            if (!ImGui::GetIO().WantCaptureKeyboard) {
                // Filter out control characters except tab, newline, carriage return, backspace
                if (wParam >= 32 || wParam == 8 || wParam == 9 || wParam == 10 ||
                    wParam == 13) {
                    g_noesisRenderPath->GetNoesisView()->Char(wParam);
                }
            }
            break;
        case WM_LBUTTONDOWN:
            // Skip game/Noesis mouse handling when ImGui wants the mouse (profiler)
            if (profilerWantsInput)
                break;
            // Forward mouse to Noesis when menu is visible, dialogue, caseboard, or camera mode
            if (g_noesisRenderPath && g_noesisRenderPath->GetNoesisView() &&
                (g_noesisRenderPath->IsMenuVisible() ||
                 g_noesisRenderPath->IsDialogueModeActive() ||
                 g_noesisRenderPath->IsCaseboardModeActive() ||
                 g_noesisRenderPath->IsCameraModeActive())) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);

                // Handle camera click (takes photo)
                if (g_noesisRenderPath->IsCameraModeActive()) {
                    g_noesisRenderPath->CameraClick(x, y);
                }
                // Handle caseboard panning
                else if (g_noesisRenderPath->IsCaseboardModeActive()) {
                    g_noesisRenderPath->CaseboardPanStart(x, y);
                }
                g_noesisRenderPath->GetNoesisView()->MouseButtonDown(
                    x, y, Noesis::MouseButton_Left);
            }
            break;
        case WM_LBUTTONUP:
            if (profilerWantsInput)
                break;
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
                g_noesisRenderPath->GetNoesisView()->MouseButtonUp(
                    x, y, Noesis::MouseButton_Left);
            }
            break;
        case WM_RBUTTONDOWN:
            if (profilerWantsInput)
                break;
            if (g_noesisRenderPath && g_noesisRenderPath->GetNoesisView() &&
                (g_noesisRenderPath->IsMenuVisible() ||
                 g_noesisRenderPath->IsDialogueModeActive() ||
                 g_noesisRenderPath->IsCaseboardModeActive())) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                g_noesisRenderPath->GetNoesisView()->MouseButtonDown(
                    x, y, Noesis::MouseButton_Right);
            }
            break;
        case WM_RBUTTONUP:
            if (profilerWantsInput)
                break;
            if (g_noesisRenderPath && g_noesisRenderPath->GetNoesisView() &&
                (g_noesisRenderPath->IsMenuVisible() ||
                 g_noesisRenderPath->IsDialogueModeActive() ||
                 g_noesisRenderPath->IsCaseboardModeActive())) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                g_noesisRenderPath->GetNoesisView()->MouseButtonUp(
                    x, y, Noesis::MouseButton_Right);
            }
            break;
        case WM_MOUSEMOVE:
            trackMouse(hWnd);
            if (profilerWantsInput)
                break;
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

                // Handle dialogue hover for testimony recording
                if (g_noesisRenderPath->IsDialogueModeActive()) {
                    g_noesisRenderPath->UpdateDialogueHover(x, y);
                }

                g_noesisRenderPath->GetNoesisView()->MouseMove(x, y);
            }
            break;
        case WM_MOUSELEAVE:
            firstTime = true;
            break;
        case WM_MOUSEWHEEL: {
            if (profilerWantsInput)
                break;
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            short delta = HIWORD(wParam);
            POINT pt = {x, y};
            ScreenToClient(hWnd, &pt);
            if (g_noesisRenderPath) {
                g_noesisRenderPath->MouseWheel(pt.x, pt.y, delta);
            }
        } break;
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

    // Initialize WickedLOJ Project before any Wicked Engine initialization
    wi::Project::create();

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

    // GrymEngine expects platform-specific subdirectory (hlsl6/ for DX12)

    wi::renderer::SetShaderPath(exeDir + "shaders/");
    wi::renderer::SetShaderPath(wi::renderer::GetShaderPath() + "hlsl6/");

    wi::scene::Scene &scene = wi::scene::GetScene();

    NoesisRenderPath noesisRenderPath;
    g_noesisRenderPath = &noesisRenderPath;
    noesisRenderPath.SetWindowHandle(hWnd);

    // Load config FIRST to set project paths (like Editor does with InitializeFromConfig)
    noesisRenderPath.gameStartup.LoadConfig();

    // Initialize application (starts async shader compilation, sets up systems)
    // This must be called before refresh_material_library, same as Editor
    application.Initialize();

    // Initialize ImGui (for profiler overlay)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &imguiIO = ImGui::GetIO();
    (void)imguiIO;
    imguiIO.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hWnd);

    IM_ASSERT(imguiIO.BackendRendererUserData == NULL &&
              "Already initialized a renderer backend!");
    ImGui_Impl_Data *bd = IM_NEW(ImGui_Impl_Data)();
    imguiIO.BackendRendererUserData = (void *)bd;
    imguiIO.BackendRendererName = "Wicked";
    imguiIO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    // Load ImGui shaders
    {
        auto savedShaderSourcePath = wi::renderer::GetShaderSourcePath();
        wi::renderer::SetShaderSourcePath(exeDir);
        wi::renderer::LoadShader(wi::graphics::ShaderStage::VS, imguiVS, "ImGuiVS.cso");
        wi::renderer::LoadShader(wi::graphics::ShaderStage::PS, imguiPS, "ImGuiPS.cso");
        wi::renderer::SetShaderSourcePath(savedShaderSourcePath);
        ImGui_Impl_CreateDeviceObjects();
    }

    wi::Project::ptr()->LoadMaterialLibrary();

    application.ActivatePath(&noesisRenderPath);

    // Set master scene (game scene will be loaded when Play Game is pressed)
    wi::Project::ptr()->master_scene = &scene;

    // Apply graphics settings from project's grym_project.ini [graphics] section
    // NOTE: Full ApplyProjectGraphicsConfig() breaks Noesis input (buttons stop responding).
    // So we selectively apply only the visual settings that don't affect render targets/MSAA/buffers.
    {
        std::string configPath = wi::Project::ptr()->path + "grym_project.ini";
        wi::config::File projConfig;
        if (projConfig.Open(configPath.c_str())) {
            wi::GraphicsConfig gfxConfig;
            gfxConfig.Load(projConfig.GetSection("graphics"));

            // Shadow resolution (critical for directional light visibility)
            int shadow2DRes = 0;
            switch (gfxConfig.shadowProps2DIndex) {
            case 1: shadow2DRes = 128; break;
            case 2: shadow2DRes = 256; break;
            case 3: shadow2DRes = 512; break;
            case 4: shadow2DRes = 1024; break;
            case 5: shadow2DRes = 2048; break;
            case 6: shadow2DRes = 4096; break;
            }
            if (shadow2DRes > 0) wi::renderer::SetShadowProps2D(shadow2DRes);

            int shadowCubeRes = 0;
            switch (gfxConfig.shadowPropsCubeIndex) {
            case 1: shadowCubeRes = 128; break;
            case 2: shadowCubeRes = 256; break;
            case 3: shadowCubeRes = 512; break;
            case 4: shadowCubeRes = 1024; break;
            case 5: shadowCubeRes = 2048; break;
            }
            if (shadowCubeRes > 0) wi::renderer::SetShadowPropsCube(shadowCubeRes);

            // Tonemap & exposure (critical for brightness)
            noesisRenderPath.setTonemap((wi::renderer::Tonemap)gfxConfig.tonemapIndex);
            noesisRenderPath.setExposure(gfxConfig.exposure);
            noesisRenderPath.setBrightness(gfxConfig.brightness);
            noesisRenderPath.setContrast(gfxConfig.contrast);
            noesisRenderPath.setSaturation(gfxConfig.saturation);

            // Post-processing (visual quality)
            noesisRenderPath.setBloomEnabled(gfxConfig.bloomEnabled);
            noesisRenderPath.setBloomThreshold(gfxConfig.bloomThreshold);
            noesisRenderPath.setLightShaftsEnabled(gfxConfig.lightShaftsEnabled);
            noesisRenderPath.setLightShaftsStrength(gfxConfig.lightShaftsStrength);
            noesisRenderPath.setLensFlareEnabled(gfxConfig.lensFlareEnabled);
            noesisRenderPath.setColorGradingEnabled(gfxConfig.colorGradingEnabled);
            noesisRenderPath.setDitherEnabled(gfxConfig.ditherEnabled);

            // AO
            noesisRenderPath.setAO((wi::RenderPath3D::AO)gfxConfig.aoIndex);
            noesisRenderPath.setAOPower(gfxConfig.aoPower);
            noesisRenderPath.setAORange(gfxConfig.aoRange);

            // Screen space shadows
            wi::renderer::SetScreenSpaceShadowsEnabled(gfxConfig.screenSpaceShadowsEnabled);
            noesisRenderPath.setScreenSpaceShadowRange(gfxConfig.screenSpaceShadowsRange);
            noesisRenderPath.setScreenSpaceShadowSampleCount(gfxConfig.screenSpaceShadowsStepCount);

            // SSR
            noesisRenderPath.setSSREnabled(gfxConfig.ssrEnabled);

            // Eye adaption
            noesisRenderPath.setEyeAdaptionEnabled(gfxConfig.eyeAdaptionEnabled);
            noesisRenderPath.setEyeAdaptionKey(gfxConfig.eyeAdaptionKey);
            noesisRenderPath.setEyeAdaptionRate(gfxConfig.eyeAdaptionRate);

            // GI
            wi::renderer::SetGIBoost(gfxConfig.giBoost);
            wi::renderer::SetDDGIEnabled(gfxConfig.ddgiEnabled);

            // Capsule shadows
            wi::renderer::SetCapsuleShadowEnabled(gfxConfig.capsuleShadowsEnabled);
            wi::renderer::SetCapsuleShadowFade(gfxConfig.capsuleShadowFade);

            wi::backlog::post("Applied graphics settings from project config (Noesis-safe subset)\n");
        }
    }

    // IMPORTANT: Sync the global camera with scene camera to prevent terrain reset

    // RenderPath3D copies its camera to scene.camera on every frame, so we need

    // the global camera to match the spawn position set in LoadGameScene

    wi::scene::GetCamera() = scene.camera;

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
            bool shortcut_handled = false;
            if (msg.message == WM_KEYDOWN) {
                if ((msg.lParam & 0x40000000) == 0) { // Not repeated
                    shortcut_handled = g_noesisRenderPath->TryHandleShortcut(
                        ConvertWin32KeyToNoesis((int)msg.wParam));
                }
            }

            if (!shortcut_handled) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

        } else {
            application.Run();
        }
    }

    wi::jobsystem::ShutDown(); // waits for jobs to finish before shutdown

    // Shutdown ImGui
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // Destroy WickedLOJ Project at the end

    wi::Project::destroy();

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
