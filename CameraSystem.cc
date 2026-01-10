#include "CameraSystem.h"
#include "CaseboardSystem.h"

#include "Utility/stb_image_write.h"

#include <NsGui/Enums.h>

#include <algorithm>
#include <cmath>

void CameraSystem::Initialize(Noesis::Grid *panel, Noesis::FrameworkElement *shutterTop,
                              Noesis::FrameworkElement *shutterBottom, Noesis::TextBlock *photoCount,
                              HWND hwnd) {
    cameraPanel = Noesis::Ptr<Noesis::Grid>(panel);
    shutterBarTop = Noesis::Ptr<Noesis::FrameworkElement>(shutterTop);
    shutterBarBottom = Noesis::Ptr<Noesis::FrameworkElement>(shutterBottom);
    cameraPhotoCount = Noesis::Ptr<Noesis::TextBlock>(photoCount);
    windowHandle = hwnd;
}

void CameraSystem::Shutdown() {
    cameraPanel.Reset();
    shutterBarTop.Reset();
    shutterBarBottom.Reset();
    cameraPhotoCount.Reset();
}

void CameraSystem::EnterCameraMode(wi::ecs::Entity playerEntity, wi::scene::Scene &scene) {
    if (inCameraMode)
        return;

    inCameraMode = true;

    // Hide player character
    hiddenPlayerObjects.clear();
    if (playerEntity != wi::ecs::INVALID_ENTITY) {
        // Hide the player's own object component
        wi::scene::ObjectComponent *playerObject = scene.objects.GetComponent(playerEntity);
        if (playerObject) {
            playerObject->SetRenderable(false);
            hiddenPlayerObjects.push_back(playerEntity);
        }

        // Hide all child entities
        for (size_t i = 0; i < scene.hierarchy.GetCount(); i++) {
            wi::ecs::Entity entity = scene.hierarchy.GetEntity(i);
            wi::scene::HierarchyComponent *hierarchy = scene.hierarchy.GetComponent(entity);

            wi::ecs::Entity parent = hierarchy->parentID;
            bool isPlayerDescendant = false;
            while (parent != wi::ecs::INVALID_ENTITY) {
                if (parent == playerEntity) {
                    isPlayerDescendant = true;
                    break;
                }
                wi::scene::HierarchyComponent *parentHierarchy =
                    scene.hierarchy.GetComponent(parent);
                parent = parentHierarchy ? parentHierarchy->parentID : wi::ecs::INVALID_ENTITY;
            }

            if (isPlayerDescendant) {
                wi::scene::ObjectComponent *childObject = scene.objects.GetComponent(entity);
                if (childObject) {
                    childObject->SetRenderable(false);
                    hiddenPlayerObjects.push_back(entity);
                }
            }
        }

        char buf[128];
        sprintf_s(buf, "EnterCameraMode: Hidden %zu player objects\n", hiddenPlayerObjects.size());
        wi::backlog::post(buf);
    }

    // Show camera panel
    if (cameraPanel) {
        cameraPanel->SetVisibility(Noesis::Visibility_Visible);
    }

    UpdateCameraPhotoCount();
    UpdateViewfinderLayout();

    // Notify callback
    if (modeChangeCallback) {
        modeChangeCallback(true);
    }

    wi::backlog::post("Entered camera mode\n");
}

void CameraSystem::ExitCameraMode(wi::scene::Scene &scene) {
    if (!inCameraMode)
        return;

    inCameraMode = false;

    // Restore player character visibility
    if (!hiddenPlayerObjects.empty()) {
        for (wi::ecs::Entity entity : hiddenPlayerObjects) {
            wi::scene::ObjectComponent *object = scene.objects.GetComponent(entity);
            if (object) {
                object->SetRenderable(true);
            }
        }
        char buf[128];
        sprintf_s(buf, "ExitCameraMode: Restored %zu player objects\n", hiddenPlayerObjects.size());
        wi::backlog::post(buf);
        hiddenPlayerObjects.clear();
    }

    // Hide camera panel
    if (cameraPanel) {
        cameraPanel->SetVisibility(Noesis::Visibility_Collapsed);
    }

    // Notify callback
    if (modeChangeCallback) {
        modeChangeCallback(false);
    }

    wi::backlog::post("Exited camera mode\n");
}

void CameraSystem::TakePhoto() {
    if (!inCameraMode || shutterActive)
        return;

    wi::backlog::post("Taking photo...\n");

    captureRequestPending = true;

    shutterActive = true;
    shutterTime = 0.0f;
    photoCapturedThisShutter = false;
}

void CameraSystem::CaptureFrameToMemory(wi::RenderPath3D *renderPath) {
    wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
    if (!device) {
        wi::backlog::post("CaptureFrameToMemory: No graphics device!",
                          wi::backlog::LogLevel::Error);
        return;
    }

    const wi::graphics::Texture *renderResultPtr = renderPath->GetLastPostprocessRT();
    if (!renderResultPtr || !renderResultPtr->IsValid()) {
        renderResultPtr = &renderPath->GetRenderResult();
    }
    if (!renderResultPtr || !renderResultPtr->IsValid()) {
        wi::backlog::post("CaptureFrameToMemory: No valid render result",
                          wi::backlog::LogLevel::Error);
        return;
    }

    const wi::graphics::Texture &renderResult = *renderResultPtr;
    wi::graphics::TextureDesc texDesc = renderResult.GetDesc();

    // Step 1: Capture raw GPU data
    wi::vector<uint8_t> rawData;
    if (!wi::helper::saveTextureToMemory(renderResult, rawData)) {
        wi::backlog::post("CaptureFrameToMemory: Failed to read texture",
                          wi::backlog::LogLevel::Error);
        return;
    }

    // Step 2: Crop to center 1/3
    uint32_t cropWidth = texDesc.width / 3;
    uint32_t cropHeight = texDesc.height / 3;
    uint32_t cropX = texDesc.width / 3;
    uint32_t cropY = texDesc.height / 3;

    const uint32_t bytesPerPixel = wi::graphics::GetFormatStride(texDesc.format);
    wi::vector<uint8_t> croppedRawData;
    CropRawData(rawData, texDesc.width, texDesc.height, bytesPerPixel, cropX, cropY, cropWidth,
                cropHeight, croppedRawData);

    // Step 3: Convert to RGBA8
    std::vector<uint8_t> rgba8Data;
    if (!ConvertToRGBA8(croppedRawData, texDesc, cropWidth, cropHeight, rgba8Data)) {
        wi::backlog::post("CaptureFrameToMemory: Format conversion failed",
                          wi::backlog::LogLevel::Error);
        return;
    }

    // Step 4: Downsample by 4x
    std::vector<uint8_t> downsampledData;
    uint32_t finalWidth, finalHeight;
    DownsampleRGBA8(rgba8Data, cropWidth, cropHeight, 4, downsampledData, finalWidth, finalHeight);

    // Step 5: Apply post-processing
    ApplySepia(downsampledData, finalWidth, finalHeight);
    AddFilmGrain(downsampledData, finalWidth, finalHeight);

    // Step 6: Write PNG
    CreateDirectoryA("SavedGameData", NULL);
    CreateDirectoryA("SavedGameData/Photos", NULL);
    photosTaken++;
    const std::string photo_filename =
        "SavedGameData/Photos/photo_" + std::to_string(photosTaken) + ".png";

    if (!stbi_write_png(photo_filename.c_str(), finalWidth, finalHeight, 4, downsampledData.data(),
                        finalWidth * 4)) {
        wi::backlog::post("CaptureFrameToMemory: Failed to write PNG",
                          wi::backlog::LogLevel::Error);
        return;
    }

    UpdateCameraPhotoCount();

    // Add to caseboard if available
    if (caseboardSystem) {
        caseboardSystem->AddPhotoCard(photo_filename, photosTaken);
    }
}

void CameraSystem::SimulateShutter(float deltaSeconds) {
    if (!shutterActive)
        return;

    shutterTime += deltaSeconds;

    // Calculate progress (0 to 1)
    float t = shutterTime / shutterDuration;

    if (t >= 1.0f) {
        shutterActive = false;
        shutterAlpha = 0.0f;
        photoCapturedThisShutter = false;
    } else {
        // Triangle wave: 0 -> 1 -> 0
        if (t < 0.5f) {
            shutterAlpha = t * 2.0f;
        } else {
            shutterAlpha = (1.0f - t) * 2.0f;
        }

        // Process photo at midpoint
        if (t >= 0.5f && !photoCapturedThisShutter) {
            photoCapturedThisShutter = true;
        }
    }

    UpdateShutterBars();
}

void CameraSystem::UpdateShutterBars() {
    if (!windowHandle)
        return;

    RECT clientRect;
    GetClientRect(windowHandle, &clientRect);
    float viewportHeight = (float)(clientRect.bottom - clientRect.top);

    // Max height is half the viewport (meet in the middle when fully closed)
    float maxHeight = viewportHeight * 0.5f;
    float barHeight = maxHeight * shutterAlpha;

    if (shutterBarTop) {
        shutterBarTop->SetHeight(barHeight);
    }
    if (shutterBarBottom) {
        shutterBarBottom->SetHeight(barHeight);
    }
}

void CameraSystem::UpdateCameraPhotoCount() {
    if (!cameraPhotoCount)
        return;

    char buf[64];
    sprintf_s(buf, "PHOTOS: %d", photosTaken);
    cameraPhotoCount->SetText(buf);
}

void CameraSystem::UpdateViewfinderLayout() {
    // Placeholder for viewfinder corner bracket positioning
}

void CameraSystem::CameraClick(int x, int y) {
    if (!inCameraMode)
        return;

    TakePhoto();
}

// Image processing helpers
void CameraSystem::CropRawData(const wi::vector<uint8_t> &srcData, uint32_t srcWidth,
                               uint32_t srcHeight, uint32_t bytesPerPixel, uint32_t cropX,
                               uint32_t cropY, uint32_t cropWidth, uint32_t cropHeight,
                               wi::vector<uint8_t> &dstData) {
    dstData.resize(cropWidth * cropHeight * bytesPerPixel);

    for (uint32_t y = 0; y < cropHeight; ++y) {
        const uint8_t *srcRow =
            srcData.data() + (cropY + y) * srcWidth * bytesPerPixel + cropX * bytesPerPixel;
        uint8_t *dstRow = dstData.data() + y * cropWidth * bytesPerPixel;
        std::memcpy(dstRow, srcRow, cropWidth * bytesPerPixel);
    }
}

bool CameraSystem::ConvertToRGBA8(const wi::vector<uint8_t> &rawData,
                                  const wi::graphics::TextureDesc &desc, uint32_t width,
                                  uint32_t height, std::vector<uint8_t> &rgba8Data) {
    using namespace wi::graphics;

    const uint32_t pixelCount = width * height;
    rgba8Data.resize(pixelCount * 4);

    if (desc.format == Format::R8G8B8A8_UNORM || desc.format == Format::R8G8B8A8_UNORM_SRGB) {
        std::memcpy(rgba8Data.data(), rawData.data(), pixelCount * 4);
        return true;
    } else if (desc.format == Format::B8G8R8A8_UNORM ||
               desc.format == Format::B8G8R8A8_UNORM_SRGB) {
        const uint32_t *src = (const uint32_t *)rawData.data();
        uint32_t *dst = (uint32_t *)rgba8Data.data();
        for (uint32_t i = 0; i < pixelCount; ++i) {
            uint32_t pixel = src[i];
            uint8_t b = (pixel >> 0) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t a = (pixel >> 24) & 0xFF;
            dst[i] = r | (g << 8) | (b << 16) | (a << 24);
        }
        return true;
    } else if (desc.format == Format::R16G16B16A16_FLOAT) {
        const XMHALF4 *src = (const XMHALF4 *)rawData.data();
        uint32_t *dst = (uint32_t *)rgba8Data.data();
        for (uint32_t i = 0; i < pixelCount; ++i) {
            float r = std::clamp(XMConvertHalfToFloat(src[i].x), 0.0f, 1.0f);
            float g = std::clamp(XMConvertHalfToFloat(src[i].y), 0.0f, 1.0f);
            float b = std::clamp(XMConvertHalfToFloat(src[i].z), 0.0f, 1.0f);
            float a = std::clamp(XMConvertHalfToFloat(src[i].w), 0.0f, 1.0f);
            dst[i] = ((uint32_t)(r * 255) << 0) | ((uint32_t)(g * 255) << 8) |
                     ((uint32_t)(b * 255) << 16) | ((uint32_t)(a * 255) << 24);
        }
        return true;
    } else if (desc.format == Format::R10G10B10A2_UNORM) {
        const uint32_t *src = (const uint32_t *)rawData.data();
        uint32_t *dst = (uint32_t *)rgba8Data.data();
        for (uint32_t i = 0; i < pixelCount; ++i) {
            uint32_t pixel = src[i];
            uint8_t r = (uint8_t)(((pixel >> 0) & 1023) * 255 / 1023);
            uint8_t g = (uint8_t)(((pixel >> 10) & 1023) * 255 / 1023);
            uint8_t b = (uint8_t)(((pixel >> 20) & 1023) * 255 / 1023);
            uint8_t a = (uint8_t)(((pixel >> 30) & 3) * 255 / 3);
            dst[i] = r | (g << 8) | (b << 16) | (a << 24);
        }
        return true;
    } else if (desc.format == Format::R11G11B10_FLOAT) {
        const XMFLOAT3PK *src = (const XMFLOAT3PK *)rawData.data();
        uint32_t *dst = (uint32_t *)rgba8Data.data();
        for (uint32_t i = 0; i < pixelCount; ++i) {
            XMFLOAT3PK pixel = src[i];
            XMVECTOR V = XMLoadFloat3PK(&pixel);
            XMFLOAT3 pixel3;
            XMStoreFloat3(&pixel3, V);
            float r = std::clamp(pixel3.x, 0.0f, 1.0f);
            float g = std::clamp(pixel3.y, 0.0f, 1.0f);
            float b = std::clamp(pixel3.z, 0.0f, 1.0f);
            dst[i] = ((uint32_t)(r * 255) << 0) | ((uint32_t)(g * 255) << 8) |
                     ((uint32_t)(b * 255) << 16) | (255 << 24);
        }
        return true;
    }

    return false;
}

void CameraSystem::DownsampleRGBA8(const std::vector<uint8_t> &srcData, uint32_t srcWidth,
                                   uint32_t srcHeight, uint32_t downsampleFactor,
                                   std::vector<uint8_t> &dstData, uint32_t &outWidth,
                                   uint32_t &outHeight) {
    outWidth = srcWidth / downsampleFactor;
    outHeight = srcHeight / downsampleFactor;
    dstData.resize(outWidth * outHeight * 4);

    for (uint32_t dstY = 0; dstY < outHeight; ++dstY) {
        for (uint32_t dstX = 0; dstX < outWidth; ++dstX) {
            uint32_t sumR = 0, sumG = 0, sumB = 0, sumA = 0;
            uint32_t sampleCount = 0;

            for (uint32_t dy = 0; dy < downsampleFactor; ++dy) {
                for (uint32_t dx = 0; dx < downsampleFactor; ++dx) {
                    uint32_t srcX = dstX * downsampleFactor + dx;
                    uint32_t srcY = dstY * downsampleFactor + dy;

                    if (srcX < srcWidth && srcY < srcHeight) {
                        uint32_t srcIdx = (srcY * srcWidth + srcX) * 4;
                        sumR += srcData[srcIdx + 0];
                        sumG += srcData[srcIdx + 1];
                        sumB += srcData[srcIdx + 2];
                        sumA += srcData[srcIdx + 3];
                        sampleCount++;
                    }
                }
            }

            if (sampleCount > 0) {
                uint32_t dstIdx = (dstY * outWidth + dstX) * 4;
                dstData[dstIdx + 0] = (uint8_t)(sumR / sampleCount);
                dstData[dstIdx + 1] = (uint8_t)(sumG / sampleCount);
                dstData[dstIdx + 2] = (uint8_t)(sumB / sampleCount);
                dstData[dstIdx + 3] = (uint8_t)(sumA / sampleCount);
            }
        }
    }
}

void CameraSystem::ApplySepia(std::vector<uint8_t> &pixels, int width, int height) {
    for (int i = 0; i < width * height; i++) {
        int idx = i * 4;

        float r = pixels[idx + 0] / 255.0f;
        float g = pixels[idx + 1] / 255.0f;
        float b = pixels[idx + 2] / 255.0f;

        // Sepia matrix transformation
        float newR = r * 0.393f + g * 0.769f + b * 0.189f;
        float newG = r * 0.349f + g * 0.686f + b * 0.168f;
        float newB = r * 0.272f + g * 0.534f + b * 0.131f;

        // Clamp and convert back to bytes
        pixels[idx + 0] = (uint8_t)(std::min(newR, 1.0f) * 255.0f);
        pixels[idx + 1] = (uint8_t)(std::min(newG, 1.0f) * 255.0f);
        pixels[idx + 2] = (uint8_t)(std::min(newB, 1.0f) * 255.0f);
    }
}

void CameraSystem::AddFilmGrain(std::vector<uint8_t> &pixels, int width, int height) {
    // Simple pseudo-random grain
    for (int i = 0; i < width * height; i++) {
        int idx = i * 4;

        // Generate noise (-16 to +16)
        int noise = (rand() % 33) - 16;

        // Add noise to each channel
        int r = pixels[idx + 0] + noise;
        int g = pixels[idx + 1] + noise;
        int b = pixels[idx + 2] + noise;

        // Clamp
        pixels[idx + 0] = (uint8_t)std::clamp(r, 0, 255);
        pixels[idx + 1] = (uint8_t)std::clamp(g, 0, 255);
        pixels[idx + 2] = (uint8_t)std::clamp(b, 0, 255);
    }
}

bool CameraSystem::SaveProcessedPhoto(const std::string &filename, const std::vector<uint8_t> &pixels,
                                      int width, int height) {
    wi::graphics::TextureDesc photoDesc;
    photoDesc.width = width;
    photoDesc.height = height;
    photoDesc.format = wi::graphics::Format::R8G8B8A8_UNORM;
    photoDesc.mip_levels = 1;
    photoDesc.array_size = 1;

    wi::vector<uint8_t> wiPixels(pixels.begin(), pixels.end());
    return wi::helper::saveTextureToFile(wiPixels, photoDesc, filename);
}
