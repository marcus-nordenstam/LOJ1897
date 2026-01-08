#pragma once

#include <Windows.h>

#include "WickedEngine.h"
#include <wiGraphicsDevice_DX12.h>
#include <wiAudio.h>

#include <NsRender/D3D12Factory.h>
#include <NsRender/RenderDevice.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/IView.h>
#include <NsGui/IRenderer.h>
#include <NsGui/UserControl.h>
#include <NsGui/InputEnums.h>
#include <NsGui/SolidColorBrush.h>
#include <NsGui/Grid.h>
#include <NsGui/TextBox.h>
#include <NsGui/Button.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/Nullable.h>
#include <NsDrawing/Color.h>
#include <NsGui/Uri.h>
#include <NsApp/LocalFontProvider.h>
#include <NsApp/LocalXamlProvider.h>
#include <NsApp/LocalTextureProvider.h>

#include <chrono>
#include <string>
#include <fstream>
#include <vector>

// Noesis integration for Wicked Engine
class NoesisRenderPath : public wi::RenderPath3D {
private:
    Noesis::Ptr<Noesis::RenderDevice> noesisDevice;
    Noesis::Ptr<Noesis::IView> uiView;
    Noesis::Ptr<Noesis::TextBox> seedTextBox; // Seed input
    Noesis::Ptr<Noesis::Button> playGameButton; // Play game button
    Noesis::Ptr<Noesis::Button> fullscreenButton; // Fullscreen toggle button
    Noesis::Ptr<Noesis::FrameworkElement> rootElement; // Root element from XAML
    ID3D12Fence *frameFence = nullptr;
    uint64_t startTime = 0;
    
    // Saved settings
    std::string projectPath;
    std::string themeMusic;
    std::string levelPath;
    std::string playerModel;
    std::string npcModel;
    
    // Music playback
    wi::audio::Sound menuMusic;
    wi::audio::SoundInstance menuMusicInstance;
    
    // UI state
    bool menuVisible = true;
    
    // Fullscreen state
    HWND windowHandle = nullptr;
    bool isFullscreen = false;
    RECT windowedRect = {};
    DWORD windowedStyle = 0;
    DWORD windowedExStyle = 0;

public:
    // Set window handle for fullscreen management
    void SetWindowHandle(HWND hwnd) { windowHandle = hwnd; }
    
    // Get Noesis view for input handling
    Noesis::IView *GetNoesisView() const { return uiView; }
    
    // Config file management
    std::string GetConfigFilePath() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);

        // Extract directory
        std::wstring path = exePath;
        size_t lastSlash = path.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            path = path.substr(0, lastSlash + 1);
        }
        path += L"config.ini";

        // Convert wide string to UTF-8
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, NULL, 0, NULL, NULL);
        if (size_needed > 0) {
            std::vector<char> buffer(size_needed);
            WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, buffer.data(), size_needed, NULL, NULL);
            return std::string(buffer.data());
        }
        return "config.ini"; // Fallback
    }

    void SaveConfig() {
        std::string configPath = GetConfigFilePath();
        std::ofstream configFile(configPath);
        if (configFile.is_open()) {
            configFile << "project_path = " << projectPath << "\n";
            configFile << "theme_music = " << themeMusic << "\n";
            configFile << "level = " << levelPath << "\n";
            configFile << "player_model = " << playerModel << "\n";
            configFile << "npc_model = " << npcModel << "\n";
            configFile.close();
            
            char buffer[512];
            sprintf_s(buffer, "Saved config: project_path = %s\n", projectPath.c_str());
            OutputDebugStringA(buffer);
        }
    }

    void LoadConfig() {
        std::string configPath = GetConfigFilePath();
        std::ifstream configFile(configPath);
        if (configFile.is_open()) {
            std::string line;
            while (std::getline(configFile, line)) {
                // Parse INI format: key = value
                size_t equalsPos = line.find('=');
                if (equalsPos != std::string::npos) {
                    std::string key = line.substr(0, equalsPos);
                    std::string value = line.substr(equalsPos + 1);
                    
                    // Trim whitespace from key and value
                    key.erase(0, key.find_first_not_of(" \t\r\n"));
                    key.erase(key.find_last_not_of(" \t\r\n") + 1);
                    value.erase(0, value.find_first_not_of(" \t\r\n"));
                    value.erase(value.find_last_not_of(" \t\r\n") + 1);
                    
                    // Remove quotes from value if present
                    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                        value = value.substr(1, value.size() - 2);
                    }
                    
                    if (key == "project_path") {
                        projectPath = value;
                    } else if (key == "theme_music") {
                        themeMusic = value;
                    } else if (key == "level") {
                        levelPath = value;
                    } else if (key == "player_model") {
                        playerModel = value;
                    } else if (key == "npc_model") {
                        npcModel = value;
                    }
                }
            }
            configFile.close();
            
            char buffer[512];
            sprintf_s(buffer, "Loaded config:\n  project_path = %s\n  theme_music = %s\n  level = %s\n", 
                     projectPath.c_str(), themeMusic.c_str(), levelPath.c_str());
            OutputDebugStringA(buffer);
        }
        
        // Update control states based on project path
        UpdateControlStates();
    }
    
    void UpdateControlStates() {
        // Check if project path is set from config file
        bool hasProjectPath = !projectPath.empty();
        
        // Enable/disable controls based on project path
        if (seedTextBox) {
            seedTextBox->SetIsEnabled(hasProjectPath);
        }
        if (playGameButton) {
            playGameButton->SetIsEnabled(hasProjectPath);
        }
        if (fullscreenButton) {
            fullscreenButton->SetIsEnabled(hasProjectPath);
        }
        
        // Load and play music if project path is set and music isn't already playing
        if (hasProjectPath && !menuMusicInstance.IsValid()) {
            LoadAndPlayMenuMusic();
        }
        // Stop music if project path is cleared
        else if (!hasProjectPath && menuMusicInstance.IsValid()) {
            wi::audio::Stop(&menuMusicInstance);
            menuMusicInstance = {};
            menuMusic = {};
        }
    }
    
    void LoadAndPlayMenuMusic() {
        // Use theme_music from config, or fall back to default
        if (themeMusic.empty()) {
            OutputDebugStringA("No theme_music configured in config.ini\n");
            return;
        }
        
        // Construct full music file path
        // WickedEngine supports WAV, OGG Vorbis, and MP3 formats
        std::string musicPath = projectPath;
        if (!musicPath.empty() && musicPath.back() != '/' && musicPath.back() != '\\') {
            musicPath += "\\";
        }
        musicPath += themeMusic;
        
        // Normalize path separators
        for (char& c : musicPath) {
            if (c == '/') c = '\\';
        }
        
        // Check if file exists
        if (!wi::helper::FileExists(musicPath)) {
            // Try alternate formats
            std::string oggPath = wi::helper::ReplaceExtension(musicPath, "ogg");
            std::string wavPath = wi::helper::ReplaceExtension(musicPath, "wav");
            
            if (wi::helper::FileExists(oggPath)) {
                musicPath = oggPath;
            } else if (wi::helper::FileExists(wavPath)) {
                musicPath = wavPath;
            } else {
                char buffer[512];
                sprintf_s(buffer, "Music file not found: %s (tried .mp3, .ogg, .wav)\n", musicPath.c_str());
                OutputDebugStringA(buffer);
                return;
            }
        }
        
        // Create sound from file
        if (wi::audio::CreateSound(musicPath, &menuMusic)) {
            // Configure music instance
            menuMusicInstance.type = wi::audio::SUBMIX_TYPE_MUSIC;
            menuMusicInstance.SetLooped(true);
            
            // Create sound instance and play
            if (wi::audio::CreateSoundInstance(&menuMusic, &menuMusicInstance)) {
                wi::audio::Play(&menuMusicInstance);
                
                char buffer[512];
                sprintf_s(buffer, "Started playing menu music: %s\n", musicPath.c_str());
                OutputDebugStringA(buffer);
            } else {
                OutputDebugStringA("Failed to create music instance\n");
            }
        } else {
            char buffer[512];
            sprintf_s(buffer, "Failed to load music: %s\n", musicPath.c_str());
            OutputDebugStringA(buffer);
        }
    }
    
    void LoadGameScene() {
        if (levelPath.empty()) {
            OutputDebugStringA("ERROR: No level path configured in config.ini\n");
            return;
        }
        
        // Construct full scene file path
        std::string scenePath = projectPath;
        if (!scenePath.empty() && scenePath.back() != '/' && scenePath.back() != '\\') {
            scenePath += "\\";
        }
        scenePath += levelPath;
        
        // Normalize path separators
        for (char& c : scenePath) {
            if (c == '/') c = '\\';
        }
        
        char buffer[512];
        sprintf_s(buffer, "Loading scene: %s\n", scenePath.c_str());
        OutputDebugStringA(buffer);
        
        // Check if file exists
        if (!wi::helper::FileExists(scenePath)) {
            sprintf_s(buffer, "ERROR: Scene file not found: %s\n", scenePath.c_str());
            OutputDebugStringA(buffer);
            return;
        }
        
        // Get the current scene
        wi::scene::Scene& scene = wi::scene::GetScene();
        
        // Clear existing scene
        scene.Clear();
        
        // Load the new scene
        wi::Archive archive(scenePath);
        archive.SetSourceDirectory(wi::helper::InferProjectPath(scenePath));
        if (archive.IsOpen()) {
            scene.Serialize(archive);
            
            OutputDebugStringA("Scene loaded successfully\n");
            
            // Update scene to compute bounds
            scene.Update(0.0f);
            
            // Get scene bounds (automatically computed by the scene)
            XMFLOAT3 sceneMin = scene.bounds.getMin();
            XMFLOAT3 sceneMax = scene.bounds.getMax();
            
            // Calculate center point
            XMFLOAT3 center = scene.bounds.getCenter();
            
            // Calculate scene size for camera distance
            float sizeX = sceneMax.x - sceneMin.x;
            float sizeY = sceneMax.y - sceneMin.y;
            float sizeZ = sceneMax.z - sceneMin.z;
            float maxSize = std::max(std::max(sizeX, sizeY), sizeZ);
            
            // Position camera at center, offset back and up to view the scene
            float cameraDistance = maxSize * 1.5f; // 1.5x scene size for good viewing
            
            XMFLOAT3 cameraPos;
            cameraPos.x = center.x;
            cameraPos.y = center.y + maxSize * 0.3f; // Slightly elevated
            cameraPos.z = center.z + cameraDistance;
            
            // Set camera transform
            wi::scene::TransformComponent cameraTransform;
            cameraTransform.ClearTransform();
            cameraTransform.Translate(cameraPos);
            cameraTransform.UpdateTransform();
            
            // Look at the center
            XMVECTOR Eye = XMLoadFloat3(&cameraPos);
            XMVECTOR At = XMLoadFloat3(&center);
            XMVECTOR Up = XMVectorSet(0, 1, 0, 0);
            
            XMMATRIX view = XMMatrixLookAtLH(Eye, At, Up);
            XMMATRIX viewInv = XMMatrixInverse(nullptr, view);
            
            XMFLOAT4X4 worldMatrix;
            XMStoreFloat4x4(&worldMatrix, viewInv);
            cameraTransform.world = worldMatrix;
            cameraTransform.SetDirty();
            
            // Apply to camera
            camera->TransformCamera(cameraTransform);
            camera->UpdateCamera();
            
            sprintf_s(buffer, "Camera positioned at scene center: (%.2f, %.2f, %.2f)\n", 
                     cameraPos.x, cameraPos.y, cameraPos.z);
            OutputDebugStringA(buffer);
            
        } else {
            sprintf_s(buffer, "ERROR: Failed to open scene archive: %s\n", scenePath.c_str());
            OutputDebugStringA(buffer);
        }
    }
    
    // Toggle fullscreen mode
    void ToggleFullscreen() {
        if (!windowHandle) return;
        
        if (!isFullscreen) {
            // Save current window state
            windowedStyle = GetWindowLong(windowHandle, GWL_STYLE);
            windowedExStyle = GetWindowLong(windowHandle, GWL_EXSTYLE);
            GetWindowRect(windowHandle, &windowedRect);
            
            // Get monitor info
            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfo(MonitorFromWindow(windowHandle, MONITOR_DEFAULTTOPRIMARY), &mi)) {
                // Remove window decorations and set fullscreen style
                SetWindowLong(windowHandle, GWL_STYLE, windowedStyle & ~(WS_CAPTION | WS_THICKFRAME));
                SetWindowLong(windowHandle, GWL_EXSTYLE, windowedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
                
                // Set window to cover entire monitor
                SetWindowPos(windowHandle, HWND_TOP,
                    mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                
                isFullscreen = true;
                
                // Update button text
                if (fullscreenButton) {
                    fullscreenButton->SetContent("WINDOWED");
                }
            }
        } else {
            // Restore windowed mode
            SetWindowLong(windowHandle, GWL_STYLE, windowedStyle);
            SetWindowLong(windowHandle, GWL_EXSTYLE, windowedExStyle);
            
            SetWindowPos(windowHandle, HWND_NOTOPMOST,
                windowedRect.left, windowedRect.top,
                windowedRect.right - windowedRect.left,
                windowedRect.bottom - windowedRect.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            
            isFullscreen = false;
            
            // Update button text
            if (fullscreenButton) {
                fullscreenButton->SetContent("FULLSCREEN");
            }
        }
    }

    void Start() override {
        // Call parent Start first
        RenderPath3D::Start();
        setOutlineEnabled(true);
        
        // Initialize audio system
        wi::audio::Initialize();
        
        InitializeNoesis();
        LoadConfig();
    }

    void Stop() override {
        // Cleanup Noesis
        ShutdownNoesis();

        // Call parent Stop
        RenderPath3D::Stop();
    }

    void PreRender() override {
        // Call parent PreRender first (sets up Wicked Engine rendering)
        RenderPath3D::PreRender();

        // Now do Noesis offscreen rendering before main render target is bound (only if menu is visible)
        if (menuVisible && uiView && noesisDevice) {
            wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
            wi::graphics::CommandList cmd = device->BeginCommandList();

            // Get D3D12 device and command list
            wi::graphics::GraphicsDevice_DX12 *dx12Device =
                    static_cast<wi::graphics::GraphicsDevice_DX12 *>(device);
            ID3D12GraphicsCommandList *d3d12CmdList =
                    dx12Device->GetD3D12CommandList(cmd);

            // Get safe fence value (frame count + buffer count for safety)
            uint64_t safeFenceValue = device->GetFrameCount() + device->GetBufferCount() + 1;

            // Set command list for Noesis (MUST be called before any Noesis rendering)
            NoesisApp::D3D12Factory::SetCommandList(noesisDevice, d3d12CmdList, safeFenceValue);

            // Update Noesis view
            if (startTime == 0) {
                startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
            }
            uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
            double time = (currentTime - startTime) / 1000.0;
            uiView->Update(time);

            // Update render tree and render offscreen textures
            // IMPORTANT: Do this BEFORE Wicked Engine binds its main render target!
            uiView->GetRenderer()->UpdateRenderTree();
            uiView->GetRenderer()->RenderOffscreen();
        }
    }


    void Compose(wi::graphics::CommandList cmd) const override {
        // Call parent Compose first (Wicked Engine composes its layers to backbuffer)
        RenderPath3D::Compose(cmd);

        // Now render Noesis UI on top of everything (only if menu is visible)
        if (menuVisible && uiView && noesisDevice) {
            wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
            wi::graphics::GraphicsDevice_DX12 *dx12Device =
                    static_cast<wi::graphics::GraphicsDevice_DX12 *>(device);
            ID3D12GraphicsCommandList *d3d12CmdList =
                    dx12Device->GetD3D12CommandList(cmd);

            // At this point, Wicked Engine has:
            // 1. Rendered the 3D scene to offscreen textures
            // 2. Composed those textures to the backbuffer
            // 3. The backbuffer is bound as render target

            // IMPORTANT: Set command list for Noesis before rendering
            // Get safe fence value (frame count + buffer count for safety)
            uint64_t safeFenceValue = device->GetFrameCount() + device->GetBufferCount() + 1;
            NoesisApp::D3D12Factory::SetCommandList(noesisDevice, d3d12CmdList, safeFenceValue);

            // Render Noesis UI on top
            uiView->GetRenderer()->Render();

            // Complete any pending split barriers
            NoesisApp::D3D12Factory::EndPendingSplitBarriers(noesisDevice);
        }
    }

    void ResizeBuffers() override {
        // Call parent ResizeBuffers first
        RenderPath3D::ResizeBuffers();

        // Update Noesis view size
        if (uiView) {
            XMUINT2 internalResolution = GetInternalResolution();
            uiView->SetSize((float) internalResolution.x, (float) internalResolution.y);
        }
    }

    bool MouseWheel(int x, int y, int delta) {
        if (!uiView)
            return false;
        // Forward to Noesis
        return uiView->MouseWheel(x, y, delta);
    }


private:
    // Helper function to find an element by name in the visual tree
    template<typename T>
    Noesis::Ptr<T> FindElementByName(Noesis::FrameworkElement *root, const char *name) {
        if (!root)
            return Noesis::Ptr<T>();

        Noesis::BaseComponent *found = root->FindName(name);
        if (!found)
            return Noesis::Ptr<T>();

        T *casted = Noesis::DynamicCast<T *>(found);
        if (!casted)
            return Noesis::Ptr<T>();

        // Explicitly construct Ptr from raw pointer
        // Elements from XAML are already managed by the visual tree, so this is a non-owning reference
        Noesis::Ptr<T> result(casted);
        return result;
    }

    void InitializeNoesis() {
        // Initialize Noesis
        Noesis::GUI::SetLicense(NS_LICENSE_NAME, NS_LICENSE_KEY);
        Noesis::GUI::Init();

        // Set texture provider to load images from GUI folder
        Noesis::GUI::SetTextureProvider(Noesis::MakePtr<NoesisApp::LocalTextureProvider>("./GUI"));
        
        Noesis::GUI::SetXamlProvider(Noesis::MakePtr<NoesisApp::LocalXamlProvider>("./GUI"));

        Noesis::GUI::SetFontProvider(Noesis::MakePtr<NoesisApp::LocalFontProvider>("./GUI/Noesis/Data/Theme"));
        const char *fonts[] = {"Fonts/#PT Root UI", "Arial", "Segoe UI Emoji"};
        Noesis::GUI::SetFontFallbacks(fonts, 3);
        Noesis::GUI::SetFontDefaultProperties(14.0f, Noesis::FontWeight_Normal, Noesis::FontStretch_Normal,
                                              Noesis::FontStyle_Normal);

        Noesis::GUI::LoadApplicationResources("Noesis/Data/Theme/NoesisTheme.DarkBlue.xaml");

        Noesis::Uri panelUri("Panel.xaml");
        Noesis::Ptr<Noesis::FrameworkElement> panelRoot = Noesis::GUI::LoadXaml<Noesis::FrameworkElement>(panelUri);
        if (!panelRoot) {
            OutputDebugStringA("ERROR: Failed to load Panel.xaml\n");
            return;
        }

        OutputDebugStringA("Successfully loaded Panel.xaml\n");
        rootElement = panelRoot;

        // Find all UI elements by name from the XAML
        Noesis::Grid *rootGrid = Noesis::DynamicCast<Noesis::Grid *>(panelRoot.GetPtr());
        if (!rootGrid) {
            return;
        }

        // Find main menu UI elements
        seedTextBox = FindElementByName<Noesis::TextBox>(rootGrid, "SeedTextBox");
        playGameButton = FindElementByName<Noesis::Button>(rootGrid, "PlayGameButton");
        fullscreenButton = FindElementByName<Noesis::Button>(rootGrid, "FullscreenButton");

        // Wire up event handlers
        if (playGameButton) {
            playGameButton->Click() += [this](Noesis::BaseComponent *sender, const Noesis::RoutedEventArgs &args) {
                // Get seed value
                const char *seedText = seedTextBox ? seedTextBox->GetText() : "12345";
                
                char buffer[512];
                sprintf_s(buffer, "Starting game with seed: %s\n", seedText);
                OutputDebugStringA(buffer);
                
                // Hide the menu
                menuVisible = false;
                if (rootElement) {
                    rootElement->SetVisibility(Noesis::Visibility_Collapsed);
                }
                
                // Stop menu music
                if (menuMusicInstance.IsValid()) {
                    wi::audio::Stop(&menuMusicInstance);
                    menuMusicInstance = {};
                    menuMusic = {};
                }
                
                // Load the scene
                LoadGameScene();
            };
        }
        
        // Wire up fullscreen button
        if (fullscreenButton) {
            fullscreenButton->Click() += [this](Noesis::BaseComponent *sender, const Noesis::RoutedEventArgs &args) {
                ToggleFullscreen();
            };
        }
        
        // Initial control state update
        UpdateControlStates();

        // Create UserControl to wrap everything
        Noesis::Ptr<Noesis::UserControl> root = Noesis::MakePtr<Noesis::UserControl>();
        root->SetContent(panelRoot);

        // Transparent background
        Noesis::Ptr<Noesis::SolidColorBrush> transparentBrush = Noesis::MakePtr<Noesis::SolidColorBrush>();
        transparentBrush->SetColor(Noesis::Color(0, 0, 0, 0));
        root->SetBackground(transparentBrush);

        uiView = Noesis::GUI::CreateView(root);

        // Get Wicked Engine's D3D12 device
        wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
        wi::graphics::GraphicsDevice_DX12 *dx12Device =
                static_cast<wi::graphics::GraphicsDevice_DX12 *>(device);
        ID3D12Device *d3d12Device = dx12Device->GetD3D12Device();

        // Create fence for Noesis
        HRESULT hr = d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frameFence));
        if (FAILED(hr)) {
            frameFence = nullptr;
        }

        // Get format info from Wicked Engine's swap chain
        DXGI_SAMPLE_DESC sampleDesc = {1, 0};
        DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT stencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        bool sRGB = false;

        // Create Noesis device from Wicked Engine's device
        noesisDevice = NoesisApp::D3D12Factory::CreateDevice(
                d3d12Device,
                frameFence,
                colorFormat,
                stencilFormat,
                sampleDesc,
                sRGB
        );

        // Initialize renderer
        uiView->GetRenderer()->Init(noesisDevice);

        // Get window size from Wicked Engine
        XMUINT2 internalResolution = GetInternalResolution();
        uiView->SetSize((float) internalResolution.x, (float) internalResolution.y);

        // Update render tree
        uiView->GetRenderer()->UpdateRenderTree();
    }


    void ShutdownNoesis() {
        // Stop music
        if (menuMusicInstance.IsValid()) {
            wi::audio::Stop(&menuMusicInstance);
        }
        
        if (uiView) {
            uiView->GetRenderer()->Shutdown();
        }

        uiView.Reset();
        noesisDevice.Reset();
        seedTextBox.Reset();
        playGameButton.Reset();
        fullscreenButton.Reset();
        rootElement.Reset();

        if (frameFence) {
            frameFence->Release();
            frameFence = nullptr;
        }

        Noesis::GUI::Shutdown();
    }

};

