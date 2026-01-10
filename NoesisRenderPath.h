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

// Noesis integration for Wicked Engine
class NoesisRenderPath : public wi::RenderPath3D {
  private:
    Noesis::Ptr<Noesis::RenderDevice> noesisDevice;
    Noesis::Ptr<Noesis::IView> uiView;
    Noesis::Ptr<Noesis::TextBox> seedTextBox;          // Seed input
    Noesis::Ptr<Noesis::Button> playGameButton;        // Play game button
    Noesis::Ptr<Noesis::Button> fullscreenButton;      // Fullscreen toggle button
    Noesis::Ptr<Noesis::FrameworkElement> rootElement; // Root element from XAML
    Noesis::Ptr<Noesis::Grid> menuContainer;           // Menu container (hidden during gameplay)
    Noesis::Ptr<Noesis::FrameworkElement>
        talkIndicator; // Talk indicator (shown when aiming at NPC)

    // Dialogue panel elements
    Noesis::Ptr<Noesis::Grid> dialoguePanelRoot;            // Main dialogue panel container
    Noesis::Ptr<Noesis::ScrollViewer> dialogueScrollViewer; // Scrollable dialogue history
    Noesis::Ptr<Noesis::StackPanel> dialogueList;           // Container for dialogue entries
    Noesis::Ptr<Noesis::TextBox> dialogueInput;             // Text input for player dialogue
    Noesis::Ptr<Noesis::TextBlock> dialogueHintText;        // Hint text for "[T] Take Testimony"

    // Caseboard panel elements
    Noesis::Ptr<Noesis::Grid> caseboardPanel;    // Main caseboard container
    Noesis::Ptr<Noesis::Panel> caseboardContent; // Pannable content canvas
    Noesis::ScaleTransform *caseboardZoomTransform =
        nullptr; // Zoom transform (owned by TransformGroup)
    Noesis::TranslateTransform *caseboardPanTransform =
        nullptr;                                       // Pan transform (owned by TransformGroup)
    Noesis::Ptr<Noesis::TextBlock> caseboardDebugText; // Debug text overlay

    // Camera/Photography panel elements
    Noesis::Ptr<Noesis::Grid> cameraPanel;                  // Main camera container
    Noesis::Ptr<Noesis::FrameworkElement> shutterBarTop;    // Top shutter bar
    Noesis::Ptr<Noesis::FrameworkElement> shutterBarBottom; // Bottom shutter bar
    Noesis::Ptr<Noesis::TextBlock> cameraPhotoCount;        // Photo counter text

    // Note cards
    struct NoteCard {
        Noesis::Ptr<Noesis::Grid> container;      // The note card container
        Noesis::Ptr<Noesis::TextBox> textBox;     // Editable text (when editing)
        Noesis::Ptr<Noesis::TextBlock> textLabel; // Display text (when not editing)
        float boardX = 0.0f;                      // Position in board space
        float boardY = 0.0f;
        bool isEditing = false; // Currently being edited
        std::string savedText;  // The saved note text
    };
    std::vector<NoteCard> noteCards;
    int editingNoteCardIndex = -1;  // Index of note card being edited, -1 if none
    int draggingNoteCardIndex = -1; // Index of note card being dragged, -1 if none
    float dragOffsetX = 0.0f;       // Offset from card center to mouse when drag started
    float dragOffsetY = 0.0f;

    // Photo cards (captured evidence photos)
    struct PhotoCard {
        Noesis::Ptr<Noesis::Grid> container;   // The photo card container
        Noesis::Ptr<Noesis::Image> photoImage; // The captured photo
        Noesis::Ptr<Noesis::TextBlock> label;  // Photo label
        float boardX = 0.0f;                   // Position in board space
        float boardY = 0.0f;
        std::string labelText; // Label text
    };
    std::vector<PhotoCard> photoCards;

    // Captured photo texture storage (to keep textures alive)
    std::vector<Noesis::Ptr<Noesis::BitmapSource>> capturedPhotoTextures;

    ID3D12Fence *frameFence = nullptr;
    uint64_t startTime = 0;

    // Saved settings
    std::string projectPath;
    std::string themeMusic;
    std::string levelPath;
    std::string playerModel;
    std::string npcModel;
    std::string animLib;
    std::string expressionPath;

    // Animation library entities (for retargeting)
    std::vector<wi::ecs::Entity> animationLibrary;

    // Music playback
    wi::audio::Sound menuMusic;
    wi::audio::SoundInstance menuMusicInstance;

    // UI state
    bool inMainMenuMode = true;

    // Player character tracking
    wi::ecs::Entity playerCharacter = wi::ecs::INVALID_ENTITY;
    std::vector<wi::ecs::Entity> hiddenPlayerObjects; // Objects hidden during camera mode
    float cameraHorizontal = 0.0f;                    // Camera yaw angle
    float cameraVertical = 0.3f;                      // Camera pitch angle
    float cameraDistance = 2.5f;                      // Camera distance from player
    float cameraHorizontalOffset = 0.25f; // Over-the-shoulder horizontal offset (positive = right)

    // NPC tracking and Lua scripts
    std::vector<wi::ecs::Entity> npcEntities;
    bool patrolScriptLoaded = false;
    bool guardScriptLoaded = false;

    // Aim dot (reticle) - raycast from character's eye in camera direction
    bool aimDotVisible = false;
    XMFLOAT2 aimDotScreenPos = {0, 0};
    bool aimingAtNPC = false;                                 // True if aim ray hits an NPC
    wi::ecs::Entity aimedNPCEntity = wi::ecs::INVALID_ENTITY; // The NPC entity being aimed at

    // Dialogue mode state
    bool inDialogueMode = false;
    wi::ecs::Entity dialogueNPCEntity = wi::ecs::INVALID_ENTITY; // NPC we're talking to
    std::string dialogueNPCName = "NPC";                         // Name of NPC for dialogue display

    // Caseboard mode state
    bool inCaseboardMode = false;

    // Camera/Photography mode state
    bool inCameraMode = false;
    int photosTaken = 0;

    // Shutter animation state
    float shutterAlpha = 0.0f;             // 0 = open, 1 = fully closed
    bool shutterActive = false;            // Is shutter animating
    float shutterDuration = 0.15f;         // Total animation time in seconds
    float shutterTime = 0.0f;              // Internal time accumulator
    bool photoCapturedThisShutter = false; // Flag to prevent multiple captures

    // Flag to request capture at end of frame (after rendering is complete)
    bool captureRequestPending = false;

    // Caseboard pan/zoom state
    float caseboardZoom = 1.0f;
    float caseboardPanX = 0.0f;
    float caseboardPanY = 0.0f;
    bool caseboardPanning = false;
    POINT caseboardLastMousePos = {};
    POINT caseboardCurrentMousePos = {}; // Current mouse position (screen space)

    // Caseboard visible/pannable area (symmetric around origin)
    // This defines the area accessible via panning at any zoom level
    float caseboardVisibleHalfX = 3000.0f; // Can pan to see -3000 to +3000 horizontally
    float caseboardVisibleHalfY = 3000.0f; // Adjusted for aspect ratio on enter

    // Fullscreen state
    HWND windowHandle = nullptr;
    bool isFullscreen = false;
    RECT windowedRect = {};
    DWORD windowedStyle = 0;
    DWORD windowedExStyle = 0;

    // Walkabout control state (third-person with mouse capture)
    bool isFirstPersonMode = false; // "First person" refers to control style, not camera
    POINT lastMousePos = {};
    bool mouseInitialized = false;
    float mouseSensitivity = 0.002f; // Mouse sensitivity for looking around

  public:
    // Set window handle for fullscreen management
    void SetWindowHandle(HWND hwnd) { windowHandle = hwnd; }

    // Get Noesis view for input handling
    Noesis::IView *GetNoesisView() const { return uiView; }

    // Check if menu is visible (for input routing)
    bool IsMenuVisible() const { return inMainMenuMode; }

    // Check if dialogue mode is active
    bool IsDialogueModeActive() const { return inDialogueMode; }

    void EnterDialogueMode(wi::ecs::Entity npcEntity);
    void ExitDialogueMode();
    void ClearDialogue();

    void AddDialogueEntry(const std::string &speaker, const std::string &message);

    // Scroll dialogue to show newest content (at bottom)
    void ScrollDialogueToBottom();

    // Handle dialogue input submission
    void OnDialogueInputCommitted();

    // Check if caseboard mode is active
    bool IsCaseboardModeActive() const { return inCaseboardMode; }

    // Enter caseboard mode
    void EnterCaseboardMode();

    // Exit caseboard mode
    void ExitCaseboardMode();

    // Update caseboard transforms based on current pan/zoom state
    void UpdateCaseboardTransforms();

    // Update caseboard debug text with current mouse position (board space) and zoom
    void UpdateCaseboardDebugText();

    // Handle caseboard zoom (mouse wheel)
    void CaseboardZoom(int x, int y, float delta);

    // Handle caseboard pan start (mouse down)
    void CaseboardPanStart(int x, int y);

    // Handle caseboard pan end (mouse up)
    void CaseboardPanEnd();

    // Handle caseboard pan move (mouse move while dragging)
    // Clamp pan values to keep board visible within viewport
    void ClampCaseboardPan();

    void CaseboardPanMove(int x, int y);

    // Add a note card at the center of the current view
    void AddNoteCard();

    // Finalize note card editing - save text and replace TextBox with TextBlock
    void FinalizeNoteCardEdit();

    // Handle click on caseboard - finalize any active note edit
    void OnCaseboardClick();

    // Start editing an existing note card (replace TextBlock with TextBox)
    void StartEditingNoteCard(int index);

    // Check if a board position is on a note card's draggable area (not the text area)
    // Returns the index of the note card, or -1 if not on any
    int HitTestNoteCardDragArea(float boardX, float boardY);

    // Start dragging a note card
    void StartDraggingNoteCard(int index, float boardX, float boardY);

    // Update dragged note card position
    void UpdateDraggingNoteCard(float boardX, float boardY);

    // Stop dragging
    void StopDraggingNoteCard();

    // Check if currently dragging a note card
    bool IsDraggingNoteCard() const { return draggingNoteCardIndex >= 0; }

    // ========== CAMERA/PHOTOGRAPHY MODE ==========

    // Check if camera mode is active
    bool IsCameraModeActive() const { return inCameraMode; }

    // Enter camera mode
    void EnterCameraMode();

    // Exit camera mode
    void ExitCameraMode();

    // Take a photo (triggers shutter animation)
    void TakePhoto();

    // Capture the current frame to memory (before shutter obscures it)
    void CaptureFrameToMemory();
    
    // Convert raw GPU texture data to RGBA8 format with optional downsampling
    bool ConvertToRGBA8(const wi::vector<uint8_t>& rawData, 
                        const wi::graphics::TextureDesc& desc, 
                        std::vector<uint8_t>& rgba8Data,
                        uint32_t& outWidth,
                        uint32_t& outHeight,
                        uint32_t downsampleFactor = 1);

    // Apply sepia filter to pixel data (RGBA format)
    void ApplySepia(std::vector<uint8_t> &pixels, int width, int height);

    // Add film grain effect to pixel data
    void AddFilmGrain(std::vector<uint8_t> &pixels, int width, int height);

    // Save processed photo pixels to a PNG file, returns the filename
    // Save processed pixels to a PNG file
    bool SaveProcessedPhoto(const std::string &filename, const std::vector<uint8_t> &pixels,
                            int width, int height);

    // Add a photo card to the caseboard with the image from file
    void AddPhotoCard(const std::string &photoFilename);

    // Simulate shutter animation (call from Update)
    void SimulateShutter(float deltaSeconds);

    // Update shutter bar heights based on shutterAlpha
    void UpdateShutterBars();

    // Update the photo count display
    void UpdateCameraPhotoCount();

    // Update viewfinder layout based on window size
    void UpdateViewfinderLayout();

    // Handle mouse click in camera mode
    void CameraClick(int x, int y);

    bool TryHandleShortcut(Noesis::Key key);

    // Config file management
    std::string GetConfigFilePath();

    void SaveConfig();

    void LoadConfig();

    void InitializeAnimationSystem();

    void UpdateControlStates();

    void LoadAndPlayMenuMusic();

    // Helper to find animation entity by name substring (like walkabout)
    wi::ecs::Entity FindAnimationByName(const wi::scene::Scene &scene,
                                        const char *anim_name_substr);

    void SpawnCharactersFromMetadata(wi::scene::Scene &scene);

    wi::ecs::Entity SpawnCharacter(wi::scene::Scene &scene, const std::string &modelPath,
                                   const XMFLOAT3 &position, const XMFLOAT3 &forward,
                                   bool isPlayer);

    void LoadNPCScripts();

    void CleanupNPCScripts();

    void LoadGameScene();

    void SetThirdPersonMode(bool enabled);

    // Enable/disable first-person camera mode (for camera/photography mode)
    void SetFirstPersonMode(bool enabled);

    void ToggleFullscreen();

    void Start() override;

    void Stop() override;

    void Update(float dt) override;

    void PreRender() override;

    // Called after Render() but before Compose() - the render target is now filled
    void PostRender() override;

    void Compose(wi::graphics::CommandList cmd) const override;

    void ResizeBuffers() override;

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

        // Explicitly construct Ptr from raw pointer
        // Elements from XAML are already managed by the visual tree, so this is a non-owning
        // reference
        Noesis::Ptr<T> result(casted);
        return result;
    }

    void InitializeNoesis();
    void ShutdownNoesis();
};
