#pragma once

#include "WickedEngine.h"
#include <wiAudio.h>
#include <wiGraphicsDevice_DX12.h>
#include <wiHelper.h>

// Windows.h is included via WickedEngine, need it for CreateDirectoryA, DeleteFileA
#include <Windows.h>

// For stbi_load to load PNG files (stb_image_write is in wiHelper.cpp)
#include "Utility/stb_image.h"
#include "Utility/stb_image_write.h"

#include <Systems/animation_system.h>
#include <Systems/character_system.h>

#include <NsApp/LocalFontProvider.h>
#include <NsApp/LocalTextureProvider.h>
#include <NsApp/LocalXamlProvider.h>
#include <NsCore/Nullable.h>
#include <NsCore/ReflectionImplement.h>
#include <NsDrawing/Color.h>
#include <NsDrawing/Thickness.h>
#include <NsGui/BitmapImage.h>
#include <NsGui/Border.h>
#include <NsGui/Button.h>
#include <NsGui/Canvas.h>
#include <NsGui/DropShadowEffect.h>
#include <NsGui/Enums.h>
#include <NsGui/FocusManager.h>
#include <NsGui/FontFamily.h>
#include <NsGui/FontProperties.h>
#include <NsGui/Grid.h>
#include <NsGui/IRenderer.h>
#include <NsGui/IView.h>
#include <NsGui/Image.h>
#include <NsGui/InputEnums.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/Panel.h>
#include <NsGui/ScaleTransform.h>
#include <NsGui/ScrollViewer.h>
#include <NsGui/SolidColorBrush.h>
#include <NsGui/StackPanel.h>
#include <NsGui/TextBlock.h>
#include <NsGui/TextBox.h>
#include <NsGui/TextProperties.h>
#include <NsGui/TransformGroup.h>
#include <NsGui/TranslateTransform.h>
#include <NsGui/UIElement.h>
#include <NsGui/UIElementCollection.h>
#include <NsGui/Uri.h>
#include <NsGui/UserControl.h>
#include <NsRender/D3D12Factory.h>
#include <NsRender/RenderDevice.h>

#include <chrono>
#include <fstream>
#include <string>
#include <vector>

// Include the gameplay subsystems
#include "CaseboardMode.h"
#include "DialogueMode.h"
#include "GameStartup.h"
#include "PhotoMode.h"

// Noesis integration for Wicked Engine
class NoesisRenderPath : public wi::RenderPath3D {
  public:
    // Gameplay subsystems
    DialogueMode dialogueSystem;
    CaseboardMode caseboardSystem;
    PhotoMode cameraSystem;
    GameStartup gameStartup;

  private:
    Noesis::Ptr<Noesis::RenderDevice> noesisDevice;
    Noesis::Ptr<Noesis::IView> uiView;
    Noesis::Ptr<Noesis::FrameworkElement> rootElement; // Root element from XAML

    ID3D12Fence *frameFence = nullptr;
    uint64_t startTime = 0;

    // UI state
    bool inMainMenuMode = true;

    // Camera tracking (shared between modes)
    float cameraHorizontal = 0.0f;        // Camera yaw angle
    float cameraVertical = 0.3f;          // Camera pitch angle
    float cameraDistance = 2.5f;          // Camera distance from player
    float cameraHorizontalOffset = 0.25f; // Over-the-shoulder horizontal offset

    // Aim dot (reticle) - raycast from character's eye in camera direction
    bool aimDotVisible = false;
    XMFLOAT2 aimDotScreenPos = {0, 0};
    bool aimingAtNPC = false;                                 // True if aim ray hits an NPC
    wi::ecs::Entity aimedNPCEntity = wi::ecs::INVALID_ENTITY; // The NPC entity being aimed at

    // Fullscreen state
    HWND windowHandle = nullptr;

    // Walkabout control state (third-person with mouse capture)
    bool isFirstPersonMode = false; // "First person" refers to control style, not camera
    POINT lastMousePos = {};
    bool mouseInitialized = false;
    float mouseSensitivity = 0.002f; // Mouse sensitivity for looking around

    // Notification system
    Noesis::Ptr<Noesis::TextBlock> notificationText;
    float notificationTimer = 0.0f;
    const float notificationDuration = 3.0f; // 3 seconds

  public:
    // Set window handle for fullscreen management
    void SetWindowHandle(HWND hwnd) { windowHandle = hwnd; }

    // Get Noesis view for input handling
    Noesis::IView *GetNoesisView() const { return uiView; }

    // Check if menu is visible (for input routing)
    bool IsMenuVisible() const { return inMainMenuMode; }

    // Check if dialogue mode is active
    bool IsDialogueModeActive() const { return dialogueSystem.IsActive(); }

    // Check if caseboard mode is active
    bool IsCaseboardModeActive() const { return caseboardSystem.IsActive(); }

    // Check if camera mode is active
    bool IsCameraModeActive() const { return cameraSystem.IsActive(); }

    // Dialogue system forwarding
    void EnterDialogueMode(wi::ecs::Entity npcEntity);
    void ExitDialogueMode();
    void UpdateDialogueHover(int mouseX, int mouseY);

    // Caseboard system forwarding
    void EnterCaseboardMode();
    void ExitCaseboardMode();
    void CaseboardZoom(int x, int y, float delta);
    void CaseboardPanStart(int x, int y);
    void CaseboardPanEnd();
    void CaseboardPanMove(int x, int y);
    void AddNoteCard();

    // Camera system forwarding
    void EnterCameraMode();
    void EnterCameraModeForCaseFile(wi::ecs::Entity npcEntity);
    void ExitCameraMode();
    void TakePhoto();
    void CameraClick(int x, int y);

    // Handle keyboard shortcuts
    bool TryHandleShortcut(Noesis::Key key);

    // Third-person mode control
    void SetThirdPersonMode(bool enabled);

    // First-person camera mode (for photography)
    void SetFirstPersonMode(bool enabled);

    // Toggle fullscreen
    void ToggleFullscreen();

    // Config management (forwarded to GameStartup)
    void SaveConfig() { gameStartup.SaveConfig(); }

    // Override RenderPath3D methods
    void Start() override;
    void Stop() override;
    void Update(float dt) override;
    void PreRender() override;
    void PostRender() override;
    void Compose(wi::graphics::CommandList cmd) const override;
    void ResizeBuffers() override;

    // Mouse wheel handling
    bool MouseWheel(int x, int y, int delta);

  private:
    // Helper function to find an element by name in the visual tree
    template <typename T>
    Noesis::Ptr<T> FindElementByName(Noesis::FrameworkElement *root, const char *name) {
        if (!root)
            return Noesis::Ptr<T>();

        Noesis::BaseComponent *found = root->FindName(name);
        if (!found)
            return Noesis::Ptr<T>();

        T *casted = Noesis::DynamicCast<T *>(found);
        if (!casted)
            return Noesis::Ptr<T>();

        Noesis::Ptr<T> result(casted);
        return result;
    }

    void InitializeNoesis();
    void ShutdownNoesis();
    void ShowNotification(const char *message);
};
