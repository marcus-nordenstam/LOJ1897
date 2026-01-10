#pragma once

#include "WickedEngine.h"
#include <wiGraphicsDevice_DX12.h>

#include <NsGui/Grid.h>
#include <NsGui/TextBlock.h>

#include <Windows.h>
#include <functional>
#include <string>
#include <vector>

// Forward declarations
class NoesisRenderPath;
class CaseboardMode;

// Camera/Photography system for taking evidence photos
class PhotoMode {
  public:
    PhotoMode() = default;
    ~PhotoMode() = default;

    // Initialize with UI elements from XAML
    void Initialize(Noesis::Grid *panel, Noesis::FrameworkElement *shutterTop,
                    Noesis::FrameworkElement *shutterBottom, Noesis::TextBlock *photoCount,
                    HWND hwnd);

    // Shutdown and release UI references
    void Shutdown();

    // Set reference to caseboard system (for adding photo cards)
    void SetCaseboardSystem(CaseboardMode *caseboard) { caseboardSystem = caseboard; }

    // Enter camera mode
    void EnterCameraMode(wi::ecs::Entity playerEntity, wi::scene::Scene &scene);

    // Enter camera mode for case-file creation (with NPC info)
    void EnterCameraModeForCaseFile(wi::ecs::Entity playerEntity, wi::scene::Scene &scene,
                                    const std::string &npcName);

    // Exit camera mode
    void ExitCameraMode(wi::scene::Scene &scene);

    // Take a photo (triggers shutter animation)
    void TakePhoto();

    // Capture the current frame to memory
    void CaptureFrameToMemory(wi::RenderPath3D *renderPath);

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

    // Check if camera mode is active
    bool IsActive() const { return inCameraMode; }

    // Check if shutter is active
    bool IsShutterActive() const { return shutterActive; }

    // Check if capture is pending (for PostRender)
    bool IsCaptureRequestPending() const { return captureRequestPending; }
    void ClearCaptureRequest() { captureRequestPending = false; }

    // Check if creating case-file (for auto-exit after capture)
    bool IsCreatingCaseFile() const { return creatingCaseFile; }

    // Get photo count
    int GetPhotoCount() const { return photosTaken; }

    // Get hidden player objects (for restoring visibility)
    const std::vector<wi::ecs::Entity> &GetHiddenPlayerObjects() const {
        return hiddenPlayerObjects;
    }

    // Callback setter for mode change notifications
    using ModeChangeCallback = std::function<void(bool entering)>;
    void SetModeChangeCallback(ModeChangeCallback callback) { modeChangeCallback = callback; }

  private:
    // Image processing helpers
    void CropRawData(const wi::vector<uint8_t> &srcData, uint32_t srcWidth, uint32_t srcHeight,
                     uint32_t bytesPerPixel, uint32_t cropX, uint32_t cropY, uint32_t cropWidth,
                     uint32_t cropHeight, wi::vector<uint8_t> &dstData);

    bool ConvertToRGBA8(const wi::vector<uint8_t> &rawData, const wi::graphics::TextureDesc &desc,
                        uint32_t width, uint32_t height, std::vector<uint8_t> &rgba8Data);

    void DownsampleRGBA8(const std::vector<uint8_t> &srcData, uint32_t srcWidth, uint32_t srcHeight,
                         uint32_t downsampleFactor, std::vector<uint8_t> &dstData,
                         uint32_t &outWidth, uint32_t &outHeight);

    void ApplySepia(std::vector<uint8_t> &pixels, int width, int height);
    void AddFilmGrain(std::vector<uint8_t> &pixels, int width, int height);

    bool SaveProcessedPhoto(const std::string &filename, const std::vector<uint8_t> &pixels,
                            int width, int height);

    // UI elements
    Noesis::Ptr<Noesis::Grid> cameraPanel;
    Noesis::Ptr<Noesis::FrameworkElement> shutterBarTop;
    Noesis::Ptr<Noesis::FrameworkElement> shutterBarBottom;
    Noesis::Ptr<Noesis::TextBlock> cameraPhotoCount;

    // Window handle
    HWND windowHandle = nullptr;

    // Reference to caseboard (for adding photo cards)
    CaseboardMode *caseboardSystem = nullptr;

    // State
    bool inCameraMode = false;
    int photosTaken = 0;
    bool creatingCaseFile = false;
    std::string caseFileNPCName;

    // Hidden player objects during camera mode
    std::vector<wi::ecs::Entity> hiddenPlayerObjects;

    // Shutter animation state
    float shutterAlpha = 0.0f;
    bool shutterActive = false;
    float shutterDuration = 0.15f;
    float shutterTime = 0.0f;
    bool photoCapturedThisShutter = false;

    // Capture request flag
    bool captureRequestPending = false;

    // Callback for mode changes
    ModeChangeCallback modeChangeCallback;
};
