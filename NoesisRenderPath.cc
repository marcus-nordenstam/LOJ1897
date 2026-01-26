#pragma once

#include "NoesisRenderPath.h"

// ========== DIALOGUE SYSTEM FORWARDING ==========

void NoesisRenderPath::EnterDialogueMode(wi::ecs::Entity npcEntity) {
    if (dialogueSystem.IsActive() || caseboardSystem.IsActive() || cameraSystem.IsActive())
        return;

    wi::scene::Scene &scene = wi::scene::GetScene();
    dialogueSystem.EnterDialogueMode(npcEntity, scene, uiView);

    // Hide aim dot
    aimDotVisible = false;

    // Release mouse capture for UI interaction
    SetThirdPersonMode(false);
}

void NoesisRenderPath::ExitDialogueMode() {
    dialogueSystem.ExitDialogueMode();

    // Re-enable walkabout controls
    SetThirdPersonMode(true);
}

void NoesisRenderPath::UpdateDialogueHover(int mouseX, int mouseY) {
    dialogueSystem.UpdateDialogueHover(mouseX, mouseY);
}

// ========== CASEBOARD SYSTEM FORWARDING ==========

void NoesisRenderPath::EnterCaseboardMode() {
    if (caseboardSystem.IsActive() || dialogueSystem.IsActive() || cameraSystem.IsActive())
        return;

    caseboardSystem.EnterCaseboardMode();

    // Hide talk indicator
    dialogueSystem.SetTalkIndicatorVisible(false);

    // Hide aim dot
    aimDotVisible = false;

    // Release mouse capture for UI interaction
    SetThirdPersonMode(false);
}

void NoesisRenderPath::ExitCaseboardMode() {
    caseboardSystem.ExitCaseboardMode();

    // Re-enable walkabout controls
    SetThirdPersonMode(true);
}

void NoesisRenderPath::CaseboardZoom(int x, int y, float delta) {
    caseboardSystem.CaseboardZoom(x, y, delta);
}

void NoesisRenderPath::CaseboardPanStart(int x, int y) { caseboardSystem.CaseboardPanStart(x, y); }

void NoesisRenderPath::CaseboardPanEnd() { caseboardSystem.CaseboardPanEnd(); }

void NoesisRenderPath::CaseboardPanMove(int x, int y) { caseboardSystem.CaseboardPanMove(x, y); }

void NoesisRenderPath::AddNoteCard() { caseboardSystem.AddNoteCard(); }

// ========== CAMERA SYSTEM FORWARDING ==========

void NoesisRenderPath::EnterCameraMode() {
    if (cameraSystem.IsActive() || dialogueSystem.IsActive() || caseboardSystem.IsActive() ||
        inMainMenuMode)
        return;

    wi::scene::Scene &scene = wi::scene::GetScene();
    cameraSystem.EnterCameraMode(gameStartup.GetPlayerCharacter(), scene);

    // Hide talk indicator
    dialogueSystem.SetTalkIndicatorVisible(false);

    // Hide aim dot
    aimDotVisible = false;

    // Exit third-person mode first (to properly manage cursor visibility)
    SetThirdPersonMode(false);
    // Then switch to first-person camera
    SetFirstPersonMode(true);
}

void NoesisRenderPath::ExitCameraMode() {
    wi::scene::Scene &scene = wi::scene::GetScene();
    cameraSystem.ExitCameraMode(scene);

    // Switch back to third-person camera
    SetFirstPersonMode(false);
    SetThirdPersonMode(true);
}

void NoesisRenderPath::EnterCameraModeForCaseFile(wi::ecs::Entity npcEntity) {
    if (cameraSystem.IsActive() || dialogueSystem.IsActive() || caseboardSystem.IsActive() ||
        inMainMenuMode)
        return;

    wi::scene::Scene &scene = wi::scene::GetScene();

    // Get NPC name
    std::string npcName = "Unknown NPC";
    const wi::scene::NameComponent *nameComp = scene.names.GetComponent(npcEntity);
    if (nameComp) {
        npcName = nameComp->name;
    }

    char buf[256];
    sprintf_s(buf, "Creating case-file for NPC: %s\n", npcName.c_str());
    wi::backlog::post(buf);

    cameraSystem.EnterCameraModeForCaseFile(gameStartup.GetPlayerCharacter(), scene, npcName);

    // Hide talk indicator
    dialogueSystem.SetTalkIndicatorVisible(false);

    // Hide aim dot
    aimDotVisible = false;

    // Exit third-person mode first (to properly manage cursor visibility)
    SetThirdPersonMode(false);
    // Then switch to first-person camera
    SetFirstPersonMode(true);
}

void NoesisRenderPath::TakePhoto() { cameraSystem.TakePhoto(); }

void NoesisRenderPath::CameraClick(int x, int y) { cameraSystem.CameraClick(x, y); }

// ========== KEYBOARD SHORTCUT HANDLING ==========

bool NoesisRenderPath::TryHandleShortcut(Noesis::Key key) {
    // Check if user is editing a note card - if so, don't process shortcuts
    if (caseboardSystem.IsEditingNoteCard()) {
        // User is typing in a text box - don't process shortcuts
        return false; // Let the TextBox handle all keys
    }

    // Additional check: see if any TextBox has keyboard focus
    if (rootElement) {
        Noesis::UIElement *focusedElement = Noesis::FocusManager::GetFocusedElement(rootElement);
        if (focusedElement) {
            // Check if the focused element is a TextBox
            Noesis::TextBox *focusedTextBox =
                Noesis::DynamicCast<Noesis::TextBox *>(focusedElement);
            if (focusedTextBox) {
                // User is typing in a text box - don't process shortcuts
                return false; // Let the TextBox handle all keys
            }
        }
    }

    // Handle camera mode shortcuts
    if (cameraSystem.IsActive()) {
        switch (key) {
        case Noesis::Key_Escape:
        case Noesis::Key_Tab:
            ExitCameraMode();
            return true;
        case Noesis::Key_Space:
            TakePhoto();
            return true;
        default:
            return true; // Consume all keys in camera mode
        }
    }

    // Handle caseboard mode shortcuts
    if (caseboardSystem.IsActive()) {
        switch (key) {
        case Noesis::Key_N:
            AddNoteCard();
            return true;
        case Noesis::Key_Escape:
        case Noesis::Key_C:
            ExitCaseboardMode();
            return true;
        default:
            return false;
        }
    }

    // Handle dialogue mode shortcuts
    if (dialogueSystem.IsActive()) {
        if (key == Noesis::Key_Escape) {
            ExitDialogueMode();
            return true;
        }
        // Handle R key for recording testimony
        if (key == Noesis::Key_R && dialogueSystem.IsRecordableMessageHovered()) {
            const DialogueMode::DialogueEntry *entry = dialogueSystem.GetHoveredEntry();
            if (entry) {
                caseboardSystem.AddTestimonyCard(entry->speaker, entry->message);
                dialogueSystem
                    .MarkHoveredAsRecorded(); // Mark as recorded so it won't show indicator again
                ShowNotification("Testimony added to caseboard");
                wi::backlog::post("Recorded testimony from ");
                wi::backlog::post(entry->speaker.c_str());
                wi::backlog::post("\n");
            }
            return true;
        }
        return false;
    }

    // Normal gameplay shortcuts
    switch (key) {
    case Noesis::Key_T:
        if (aimedNPCEntity != wi::ecs::INVALID_ENTITY) {
            EnterDialogueMode(aimedNPCEntity);
        }
        return true;
    case Noesis::Key_F:
        if (aimedNPCEntity != wi::ecs::INVALID_ENTITY) {
            EnterCameraModeForCaseFile(aimedNPCEntity);
        }
        return true;
    case Noesis::Key_C:
        EnterCaseboardMode();
        return true;
    case Noesis::Key_Tab:
        EnterCameraMode();
        return true;
    default:
        return false;
    }
}

// ========== CONTROL MODES ==========

void NoesisRenderPath::SetThirdPersonMode(bool enabled) {
    if (!windowHandle)
        return;

    isFirstPersonMode = enabled;

    if (enabled) {
        ShowCursor(FALSE);

        RECT clientRect;
        GetClientRect(windowHandle, &clientRect);
        POINT center = {(clientRect.right - clientRect.left) / 2,
                        (clientRect.bottom - clientRect.top) / 2};
        ClientToScreen(windowHandle, &center);
        SetCursorPos(center.x, center.y);

        RECT windowRect;
        GetWindowRect(windowHandle, &windowRect);
        ClipCursor(&windowRect);

        mouseInitialized = false;

        wi::backlog::post(
            "Walkabout control mode enabled. Mouse captured for third-person camera.\n");
    } else {
        ShowCursor(TRUE);
        ClipCursor(nullptr);

        mouseInitialized = false;

        wi::backlog::post("Walkabout control mode disabled. Mouse released.\n");
    }
}

void NoesisRenderPath::SetFirstPersonMode(bool enabled) {
    if (!windowHandle)
        return;

    if (enabled) {
        ShowCursor(FALSE);

        RECT clientRect;
        GetClientRect(windowHandle, &clientRect);
        POINT center = {(clientRect.right - clientRect.left) / 2,
                        (clientRect.bottom - clientRect.top) / 2};
        ClientToScreen(windowHandle, &center);
        SetCursorPos(center.x, center.y);

        RECT windowRect;
        GetWindowRect(windowHandle, &windowRect);
        ClipCursor(&windowRect);

        mouseInitialized = false;

        wi::backlog::post("First-person camera mode enabled (for photography).\n");
    } else {
        ShowCursor(TRUE);
        ClipCursor(nullptr);

        mouseInitialized = false;

        wi::backlog::post("First-person camera mode disabled.\n");
    }
}

void NoesisRenderPath::ToggleFullscreen() { gameStartup.ToggleFullscreen(windowHandle); }

// ========== RENDERPATH LIFECYCLE ==========

void NoesisRenderPath::Start() {
    RenderPath3D::Start();
    setOutlineEnabled(true);

    wi::audio::Initialize();

    InitializeNoesis();

    // Note: LoadConfig() is now called earlier in main.cc before ActivatePath,
    // so project paths are set before refresh_material_library() is called.
}

void NoesisRenderPath::Stop() {
    ShutdownNoesis();
    RenderPath3D::Stop();
}

void NoesisRenderPath::Update(float dt) {
    RenderPath3D::Update(dt);

    // Update notification fade-out
    if (notificationTimer > 0.0f) {
        notificationTimer -= dt;
        if (notificationTimer <= 0.0f) {
            // Hide notification
            if (notificationText) {
                notificationText->SetVisibility(Noesis::Visibility_Collapsed);
                notificationText->SetOpacity(0.0f);
            }
        } else {
            // Fade out in the last second
            if (notificationText && notificationTimer < 1.0f) {
                notificationText->SetOpacity(notificationTimer);
            }
        }
    }

    // Run shutter animation if active
    if (cameraSystem.IsShutterActive()) {
        cameraSystem.SimulateShutter(dt);
    }

    // Skip walkabout controls while in GUI mode
    if (dialogueSystem.IsActive() || caseboardSystem.IsActive() || inMainMenuMode) {
        return;
    }

    wi::ecs::Entity playerCharacter = gameStartup.GetPlayerCharacter();

    // Handle camera mode separately (first-person camera at player's eyes)
    if (cameraSystem.IsActive() && playerCharacter != wi::ecs::INVALID_ENTITY) {
        wi::scene::Scene &scene = wi::scene::GetScene();
        wi::scene::CharacterComponent *playerChar = scene.characters.GetComponent(playerCharacter);

        if (playerChar) {
            // === MOUSE LOOK ===
            if (windowHandle) {
                RECT clientRect;
                GetClientRect(windowHandle, &clientRect);
                POINT center = {(clientRect.right - clientRect.left) / 2,
                                (clientRect.bottom - clientRect.top) / 2};
                POINT currentPos;
                GetCursorPos(&currentPos);
                ScreenToClient(windowHandle, &currentPos);

                if (mouseInitialized) {
                    float deltaX = (float)(currentPos.x - center.x) * mouseSensitivity;
                    float deltaY = (float)(currentPos.y - center.y) * mouseSensitivity;

                    if (deltaX != 0.0f || deltaY != 0.0f) {
                        cameraHorizontal += deltaX;
                        cameraVertical =
                            std::clamp(cameraVertical + deltaY, -XM_PIDIV4, XM_PIDIV4 * 1.2f);
                    }
                } else {
                    mouseInitialized = true;
                }

                POINT screenCenter = center;
                ClientToScreen(windowHandle, &screenCenter);
                SetCursorPos(screenCenter.x, screenCenter.y);
            }

            // === WASD MOVEMENT ===
            bool wPressed = (GetAsyncKeyState('W') & 0x8000) != 0;
            bool sPressed = (GetAsyncKeyState('S') & 0x8000) != 0;
            bool aPressed = (GetAsyncKeyState('A') & 0x8000) != 0;
            bool dPressed = (GetAsyncKeyState('D') & 0x8000) != 0;

            if (wPressed || sPressed || aPressed || dPressed) {
                if (playerChar->IsGrounded() || playerChar->IsWallIntersect()) {
                    XMFLOAT3 move_dir(0, 0, 0);

                    if (wPressed) {
                        move_dir.x += sinf(cameraHorizontal);
                        move_dir.z += cosf(cameraHorizontal);
                    }
                    if (sPressed) {
                        move_dir.x -= sinf(cameraHorizontal);
                        move_dir.z -= cosf(cameraHorizontal);
                    }
                    if (aPressed) {
                        move_dir.x -= cosf(cameraHorizontal);
                        move_dir.z += sinf(cameraHorizontal);
                    }
                    if (dPressed) {
                        move_dir.x += cosf(cameraHorizontal);
                        move_dir.z -= sinf(cameraHorizontal);
                    }

                    float len = sqrtf(move_dir.x * move_dir.x + move_dir.z * move_dir.z);
                    if (len > 0.001f) {
                        move_dir.x /= len;
                        move_dir.z /= len;
                    }
                    move_dir.y = 0.0f;

                    auto action =
                        wi::scene::character_system::make_walk(scene, *playerChar, move_dir);
                    playerChar->SetAction(scene, action);
                }
            } else if (playerChar->IsWalking()) {
                auto action = wi::scene::character_system::make_idle(scene, *playerChar);
                playerChar->SetAction(scene, action);
            }

            // === FIRST-PERSON CAMERA ===
            XMFLOAT3 charPos = playerChar->GetPositionInterpolated();

            XMFLOAT3 camPos;
            camPos.x = charPos.x;
            camPos.y = charPos.y + 1.6f;
            camPos.z = charPos.z;

            wi::scene::TransformComponent cameraTransform;
            cameraTransform.ClearTransform();
            cameraTransform.Translate(camPos);
            cameraTransform.RotateRollPitchYaw(XMFLOAT3(cameraVertical, cameraHorizontal, 0));
            cameraTransform.UpdateTransform();

            camera->TransformCamera(cameraTransform);
            camera->UpdateCamera();
        }
        return;
    }

    // Handle walkabout-style controls and third-person camera
    if (playerCharacter != wi::ecs::INVALID_ENTITY) {
        wi::scene::Scene &scene = wi::scene::GetScene();
        wi::scene::CharacterComponent *playerChar = scene.characters.GetComponent(playerCharacter);

        if (playerChar && isFirstPersonMode) {
            // === MOUSE LOOK ===
            if (windowHandle) {
                RECT clientRect;
                GetClientRect(windowHandle, &clientRect);
                POINT center = {(clientRect.right - clientRect.left) / 2,
                                (clientRect.bottom - clientRect.top) / 2};

                POINT currentPos;
                GetCursorPos(&currentPos);
                ScreenToClient(windowHandle, &currentPos);

                if (mouseInitialized) {
                    float deltaX = (float)(currentPos.x - center.x) * mouseSensitivity;
                    float deltaY = (float)(currentPos.y - center.y) * mouseSensitivity;

                    if (deltaX != 0.0f || deltaY != 0.0f) {
                        cameraHorizontal += deltaX;
                        cameraVertical =
                            std::clamp(cameraVertical + deltaY, -XM_PIDIV4, XM_PIDIV4 * 1.2f);
                    }
                } else {
                    mouseInitialized = true;
                }

                POINT screenCenter = center;
                ClientToScreen(windowHandle, &screenCenter);
                SetCursorPos(screenCenter.x, screenCenter.y);
            }

            // Camera zoom
            XMFLOAT4 pointer = wi::input::GetPointer();
            cameraDistance = std::max(0.5f, cameraDistance - pointer.z * 0.3f);

            // === WALK ===
            bool wPressed = (GetAsyncKeyState('W') & 0x8000) != 0;
            bool sPressed = (GetAsyncKeyState('S') & 0x8000) != 0;

            if (wPressed || sPressed) {
                if (playerChar->IsGrounded() || playerChar->IsWallIntersect()) {
                    XMFLOAT3 walk_dir;
                    if (sPressed && !wPressed) {
                        walk_dir.x = -sinf(cameraHorizontal);
                        walk_dir.y = 0.0f;
                        walk_dir.z = -cosf(cameraHorizontal);
                    } else {
                        walk_dir.x = sinf(cameraHorizontal);
                        walk_dir.y = 0.0f;
                        walk_dir.z = cosf(cameraHorizontal);
                    }

                    auto action =
                        wi::scene::character_system::make_walk(scene, *playerChar, walk_dir);
                    playerChar->SetAction(scene, action);
                }
            } else if (playerChar->IsWalking()) {
                auto action = wi::scene::character_system::make_idle(scene, *playerChar);
                playerChar->SetAction(scene, action);
            }

            // === JUMP ===
            static bool spaceWasPressed = false;
            bool spacePressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            if (spacePressed && !spaceWasPressed && playerChar->IsGrounded()) {
                auto action = wi::scene::character_system::make_jump(scene, *playerChar, 8.0f);
                playerChar->SetAction(scene, action);
            }
            spaceWasPressed = spacePressed;

            // Escape key - return to menu
            static bool escWasPressed = false;
            bool escPressed = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
            if (escPressed && !escWasPressed) {
                SetThirdPersonMode(false);
                inMainMenuMode = true;
                gameStartup.ShowMenu(true);
                gameStartup.CleanupNPCScripts();
                aimDotVisible = false;
                gameStartup.LoadAndPlayMenuMusic();
                wi::backlog::post("Returned to menu (Escape pressed)\n");
            }
            escWasPressed = escPressed;

            // === THIRD-PERSON CAMERA ===
            XMFLOAT3 charPos = playerChar->GetPositionInterpolated();

            float camRightX = cosf(cameraHorizontal);
            float camRightZ = -sinf(cameraHorizontal);

            float camOffsetX = sinf(cameraHorizontal) * cosf(cameraVertical) * cameraDistance;
            float camOffsetY = sinf(cameraVertical) * cameraDistance;
            float camOffsetZ = cosf(cameraHorizontal) * cosf(cameraVertical) * cameraDistance;

            XMFLOAT3 eyeTarget;
            eyeTarget.x = charPos.x + camRightX * cameraHorizontalOffset;
            eyeTarget.y = charPos.y + 1.6f;
            eyeTarget.z = charPos.z + camRightZ * cameraHorizontalOffset;

            XMFLOAT3 camPos;
            camPos.x = eyeTarget.x - camOffsetX;
            camPos.y = eyeTarget.y + camOffsetY;
            camPos.z = eyeTarget.z - camOffsetZ;

            // Camera collision check
            XMVECTOR targetPos = XMLoadFloat3(&eyeTarget);
            XMVECTOR cameraPos = XMLoadFloat3(&camPos);

            XMFLOAT3 rayDir;
            XMStoreFloat3(&rayDir, XMVector3Normalize(cameraPos - targetPos));
            float rayDist = XMVectorGetX(XMVector3Length(cameraPos - targetPos));

            wi::primitive::Ray cameraRay(eyeTarget, rayDir, 0.1f, rayDist);
            auto collision = scene.Intersects(
                cameraRay, wi::enums::FILTER_NAVIGATION_MESH | wi::enums::FILTER_COLLIDER, ~1u);

            if (collision.entity != wi::ecs::INVALID_ENTITY && collision.distance < rayDist) {
                XMVECTOR collisionOffset = XMLoadFloat3(&rayDir) * (collision.distance - 0.2f);
                cameraPos = targetPos + collisionOffset;
                XMStoreFloat3(&camPos, cameraPos);
            }

            wi::scene::TransformComponent cameraTransform;
            cameraTransform.ClearTransform();
            cameraTransform.Translate(camPos);
            cameraTransform.RotateRollPitchYaw(XMFLOAT3(cameraVertical, cameraHorizontal, 0));
            cameraTransform.UpdateTransform();

            camera->TransformCamera(cameraTransform);
            camera->UpdateCamera();

            // === AIM DOT & NPC DETECTION ===
            aimDotVisible = false;
            aimingAtNPC = false;

            aimDotScreenPos.x = GetLogicalWidth() * 0.5f;
            aimDotScreenPos.y = GetLogicalHeight() * 0.5f;
            aimDotVisible = true;

            XMFLOAT3 camLookDir;
            XMStoreFloat3(&camLookDir, camera->GetAt());

            wi::primitive::Ray aimRay(camPos, camLookDir, 0.1f, 100.0f);
            auto aimHit = scene.Intersects(
                aimRay, wi::enums::FILTER_OPAQUE | wi::enums::FILTER_TRANSPARENT, ~0u);

            XMFLOAT3 targetPoint;
            if (aimHit.entity != wi::ecs::INVALID_ENTITY) {
                targetPoint.x = camPos.x + camLookDir.x * aimHit.distance;
                targetPoint.y = camPos.y + camLookDir.y * aimHit.distance;
                targetPoint.z = camPos.z + camLookDir.z * aimHit.distance;
            } else {
                targetPoint.x = camPos.x + camLookDir.x * 100.0f;
                targetPoint.y = camPos.y + camLookDir.y * 100.0f;
                targetPoint.z = camPos.z + camLookDir.z * 100.0f;
            }

            // Debug line
            wi::renderer::RenderableLine debugLine;
            debugLine.start = camPos;
            debugLine.end = targetPoint;
            debugLine.color_start = XMFLOAT4(0, 1, 0, 0.3f);
            debugLine.color_end = XMFLOAT4(0, 1, 0, 0.3f);
            wi::renderer::DrawLine(debugLine);

            // Check if we hit an NPC
            aimedNPCEntity = wi::ecs::INVALID_ENTITY;
            if (aimHit.entity != wi::ecs::INVALID_ENTITY) {
                wi::ecs::Entity checkEntity = aimHit.entity;
                for (int depth = 0; depth < 10 && checkEntity != wi::ecs::INVALID_ENTITY; ++depth) {
                    const wi::scene::MindComponent *mind = scene.minds.GetComponent(checkEntity);
                    if (mind != nullptr && mind->IsNPC()) {
                        aimingAtNPC = true;
                        aimedNPCEntity = checkEntity;
                        break;
                    }
                    const wi::scene::HierarchyComponent *hier =
                        scene.hierarchy.GetComponent(checkEntity);
                    if (hier != nullptr) {
                        checkEntity = hier->parentID;
                    } else {
                        break;
                    }
                }
            }

            // Update talk indicator
            dialogueSystem.SetTalkIndicatorVisible(!inMainMenuMode && aimingAtNPC);
        }

        // Update NPC behavior
        const auto &npcEntities = gameStartup.GetNPCEntities();
        if ((gameStartup.IsPatrolScriptLoaded() || gameStartup.IsGuardScriptLoaded()) &&
            !npcEntities.empty()) {
            for (auto npc : npcEntities) {
                if (npc == wi::ecs::INVALID_ENTITY) {
                    continue;
                }
                const wi::scene::MindComponent *mind = scene.minds.GetComponent(npc);
                if (mind == nullptr || mind->scriptCallback.empty()) {
                    continue;
                }
                std::string lua_call = mind->scriptCallback + "(" + std::to_string(npc) + ", " +
                                       std::to_string(dt) + ")";
                wi::lua::RunText(lua_call);
            }
        }
    }
}

void NoesisRenderPath::PreRender() {
    RenderPath3D::PreRender();

    if (uiView && noesisDevice) {
        wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
        wi::graphics::CommandList cmd = device->BeginCommandList();

        wi::graphics::GraphicsDevice_DX12 *dx12Device =
            static_cast<wi::graphics::GraphicsDevice_DX12 *>(device);
        ID3D12GraphicsCommandList *d3d12CmdList = dx12Device->GetD3D12CommandList(cmd);

        uint64_t safeFenceValue = device->GetFrameCount() + device->GetBufferCount() + 1;

        NoesisApp::D3D12Factory::SetCommandList(noesisDevice, d3d12CmdList, safeFenceValue);

        if (startTime == 0) {
            startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
        }
        uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count();
        double time = (currentTime - startTime) / 1000.0;
        uiView->Update(time);

        uiView->GetRenderer()->UpdateRenderTree();
        uiView->GetRenderer()->RenderOffscreen();
    }
}

void NoesisRenderPath::PostRender() {
    RenderPath3D::PostRender();

    if (cameraSystem.IsCaptureRequestPending()) {
        bool wasCreatingCaseFile = cameraSystem.IsCreatingCaseFile();
        cameraSystem.ClearCaptureRequest();

        wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
        device->SubmitCommandLists();
        device->WaitForGPU();

        cameraSystem.CaptureFrameToMemory(this);

        // Auto-exit camera mode after creating case-file
        if (wasCreatingCaseFile) {
            wi::backlog::post("Auto-exiting camera mode after case-file creation\n");
            ExitCameraMode();
        }
    }
}

void NoesisRenderPath::Compose(wi::graphics::CommandList cmd) const {
    RenderPath3D::Compose(cmd);

    // Draw aim dot
    if (!inMainMenuMode && !cameraSystem.IsActive() && aimDotVisible) {
        wi::image::SetCanvas(*this);

        // Outer circle
        {
            wi::image::Params params;
            params.pos = XMFLOAT3(aimDotScreenPos.x - 4.0f, aimDotScreenPos.y - 4.0f, 0);
            params.siz = XMFLOAT2(8.0f, 8.0f);
            params.pivot = XMFLOAT2(0, 0);
            params.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            params.enableCornerRounding();
            params.corners_rounding[0] = {4.0f, 8};
            params.corners_rounding[1] = {4.0f, 8};
            params.corners_rounding[2] = {4.0f, 8};
            params.corners_rounding[3] = {4.0f, 8};
            wi::image::Draw(nullptr, params, cmd);
        }

        // Inner dot
        {
            wi::image::Params params;
            params.pos = XMFLOAT3(aimDotScreenPos.x - 2.0f, aimDotScreenPos.y - 2.0f, 0);
            params.siz = XMFLOAT2(4.0f, 4.0f);
            params.pivot = XMFLOAT2(0, 0);
            params.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            params.enableCornerRounding();
            params.corners_rounding[0] = {2.0f, 8};
            params.corners_rounding[1] = {2.0f, 8};
            params.corners_rounding[2] = {2.0f, 8};
            params.corners_rounding[3] = {2.0f, 8};
            wi::image::Draw(nullptr, params, cmd);
        }
    }

    // Render Noesis UI
    if (uiView && noesisDevice) {
        wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
        wi::graphics::GraphicsDevice_DX12 *dx12Device =
            static_cast<wi::graphics::GraphicsDevice_DX12 *>(device);
        ID3D12GraphicsCommandList *d3d12CmdList = dx12Device->GetD3D12CommandList(cmd);

        uint64_t safeFenceValue = device->GetFrameCount() + device->GetBufferCount() + 1;
        NoesisApp::D3D12Factory::SetCommandList(noesisDevice, d3d12CmdList, safeFenceValue);

        uiView->GetRenderer()->Render();

        NoesisApp::D3D12Factory::EndPendingSplitBarriers(noesisDevice);
    }
}

void NoesisRenderPath::ResizeBuffers() {
    RenderPath3D::ResizeBuffers();

    if (uiView) {
        XMUINT2 internalResolution = GetInternalResolution();
        uiView->SetSize((float)internalResolution.x, (float)internalResolution.y);
    }
}

bool NoesisRenderPath::MouseWheel(int x, int y, int delta) {
    if (!uiView)
        return false;

    if (caseboardSystem.IsActive()) {
        caseboardSystem.CaseboardZoom(x, y, (float)delta);
        return true;
    }

    // Handle third-person camera distance adjustment in normal gameplay
    if (!inMainMenuMode && !dialogueSystem.IsActive() && !cameraSystem.IsActive()) {
        // Adjust camera distance (positive delta = zoom in, negative = zoom out)
        float zoomAmount = delta / 120.0f; // Delta is typically 120 per "notch"
        cameraDistance = std::max(0.5f, cameraDistance - zoomAmount * 0.5f);
        return true;
    }

    return uiView->MouseWheel(x, y, delta);
}

void NoesisRenderPath::InitializeNoesis() {
    Noesis::GUI::SetLicense(NS_LICENSE_NAME, NS_LICENSE_KEY);
    Noesis::GUI::Init();

    Noesis::GUI::SetTextureProvider(Noesis::MakePtr<NoesisApp::LocalTextureProvider>("."));
    Noesis::GUI::SetXamlProvider(Noesis::MakePtr<NoesisApp::LocalXamlProvider>("./GUI"));
    Noesis::GUI::SetFontProvider(Noesis::MakePtr<NoesisApp::LocalFontProvider>("./GUI"));

    const char *fonts[] = {"Noesis/Data/Theme/Fonts/#PT Root UI", "Arial", "Segoe UI Emoji"};
    Noesis::GUI::SetFontFallbacks(fonts, 3);
    Noesis::GUI::SetFontDefaultProperties(14.0f, Noesis::FontWeight_Normal,
                                          Noesis::FontStretch_Normal, Noesis::FontStyle_Normal);

    Noesis::GUI::LoadApplicationResources("Noesis/Data/Theme/NoesisTheme.DarkBlue.xaml");

    Noesis::Uri panelUri("Panel.xaml");
    Noesis::Ptr<Noesis::FrameworkElement> panelRoot =
        Noesis::GUI::LoadXaml<Noesis::FrameworkElement>(panelUri);
    if (!panelRoot) {
        wi::backlog::post("ERROR: Failed to load Panel.xaml\n");
        return;
    }

    wi::backlog::post("Successfully loaded Panel.xaml\n");
    rootElement = panelRoot;

    Noesis::Grid *rootGrid = Noesis::DynamicCast<Noesis::Grid *>(panelRoot.GetPtr());
    if (!rootGrid) {
        return;
    }

    // Find UI elements and initialize subsystems
    auto menuContainer = FindElementByName<Noesis::Grid>(rootGrid, "MenuContainer");
    auto seedTextBox = FindElementByName<Noesis::TextBox>(rootGrid, "SeedTextBox");
    auto playGameButton = FindElementByName<Noesis::Button>(rootGrid, "PlayGameButton");
    auto fullscreenButton = FindElementByName<Noesis::Button>(rootGrid, "FullscreenButton");
    auto talkIndicator = FindElementByName<Noesis::FrameworkElement>(rootGrid, "TalkIndicator");

    auto dialoguePanelRoot = FindElementByName<Noesis::Grid>(rootGrid, "DialoguePanelRoot");
    auto dialogueScrollViewer =
        FindElementByName<Noesis::ScrollViewer>(rootGrid, "DialogueScrollViewer");
    auto dialogueList = FindElementByName<Noesis::StackPanel>(rootGrid, "DialogueList");
    auto dialogueInput = FindElementByName<Noesis::TextBox>(rootGrid, "DialogueInput");
    auto dialogueHintText = FindElementByName<Noesis::TextBlock>(rootGrid, "DialogueHintText");

    auto caseboardPanel = FindElementByName<Noesis::Grid>(rootGrid, "CaseboardPanel");
    auto caseboardContent = FindElementByName<Noesis::Panel>(rootGrid, "CaseboardContent");
    auto caseboardDebugText = FindElementByName<Noesis::TextBlock>(rootGrid, "CaseboardDebugText");

    auto cameraPanel = FindElementByName<Noesis::Grid>(rootGrid, "CameraPanel");
    auto shutterBarTop = FindElementByName<Noesis::FrameworkElement>(rootGrid, "ShutterBarTop");
    auto shutterBarBottom =
        FindElementByName<Noesis::FrameworkElement>(rootGrid, "ShutterBarBottom");
    auto cameraPhotoCount = FindElementByName<Noesis::TextBlock>(rootGrid, "CameraPhotoCount");
    auto recordIndicator = FindElementByName<Noesis::StackPanel>(rootGrid, "RecordIndicator");
    auto dialogueByeButton = FindElementByName<Noesis::Button>(rootGrid, "DialogueByeButton");
    notificationText = FindElementByName<Noesis::TextBlock>(rootGrid, "NotificationText");

    // Initialize subsystems
    dialogueSystem.Initialize(dialoguePanelRoot.GetPtr(), dialogueScrollViewer.GetPtr(),
                              dialogueList.GetPtr(), dialogueInput.GetPtr(),
                              dialogueHintText.GetPtr(), talkIndicator.GetPtr(),
                              recordIndicator.GetPtr(), dialogueByeButton.GetPtr());

    // Set up dialogue exit callback
    dialogueSystem.SetExitRequestCallback([this]() { ExitDialogueMode(); });

    caseboardSystem.Initialize(caseboardPanel.GetPtr(), caseboardContent.GetPtr(),
                               caseboardDebugText.GetPtr(), windowHandle);

    cameraSystem.Initialize(cameraPanel.GetPtr(), shutterBarTop.GetPtr(), shutterBarBottom.GetPtr(),
                            cameraPhotoCount.GetPtr(), windowHandle);
    cameraSystem.SetCaseboardSystem(&caseboardSystem);

    gameStartup.Initialize(menuContainer.GetPtr(), seedTextBox.GetPtr(), playGameButton.GetPtr(),
                           fullscreenButton.GetPtr());

    // Set up callbacks
    gameStartup.SetPlayGameCallback([this, menuContainer](const std::string &seed) {
        char buffer[512];
        sprintf_s(buffer, "Starting game with seed: %s\n", seed.c_str());
        wi::backlog::post(buffer);

        inMainMenuMode = false;
        if (menuContainer) {
            menuContainer->SetVisibility(Noesis::Visibility_Collapsed);
        }

        // gameStartup.StopMenuMusic();

        // Scene is already loaded during initialization, just initialize camera
        wi::scene::Scene &scene = wi::scene::GetScene();
        wi::ecs::Entity playerCharacter = gameStartup.GetPlayerCharacter();
        if (playerCharacter != wi::ecs::INVALID_ENTITY) {
            wi::scene::CharacterComponent *playerChar =
                scene.characters.GetComponent(playerCharacter);
            if (playerChar) {
                XMFLOAT3 playerForward = playerChar->GetFacing();
                cameraHorizontal = atan2f(playerForward.x, playerForward.z);
                cameraVertical = 0.3f;
                cameraDistance = 2.5f;
                wi::backlog::post("Camera initialized to follow player\n");
            }
        } else {
            wi::backlog::post("WARNING: No player character spawned, camera will not follow\n");
        }

        SetThirdPersonMode(true);
    });

    gameStartup.SetFullscreenCallback([this]() { ToggleFullscreen(); });

    gameStartup.UpdateControlStates();

    // Create UserControl wrapper
    Noesis::Ptr<Noesis::UserControl> root = Noesis::MakePtr<Noesis::UserControl>();
    root->SetContent(panelRoot);

    Noesis::Ptr<Noesis::SolidColorBrush> transparentBrush =
        Noesis::MakePtr<Noesis::SolidColorBrush>();
    transparentBrush->SetColor(Noesis::Color(0, 0, 0, 0));
    root->SetBackground(transparentBrush);

    uiView = Noesis::GUI::CreateView(root);

    // Get D3D12 device
    wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
    wi::graphics::GraphicsDevice_DX12 *dx12Device =
        static_cast<wi::graphics::GraphicsDevice_DX12 *>(device);
    ID3D12Device *d3d12Device = dx12Device->GetD3D12Device();

    HRESULT hr = d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frameFence));
    if (FAILED(hr)) {
        frameFence = nullptr;
    }

    DXGI_SAMPLE_DESC sampleDesc = {1, 0};
    DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT stencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    bool sRGB = false;

    noesisDevice = NoesisApp::D3D12Factory::CreateDevice(d3d12Device, frameFence, colorFormat,
                                                         stencilFormat, sampleDesc, sRGB);

    uiView->GetRenderer()->Init(noesisDevice);

    XMUINT2 internalResolution = GetInternalResolution();
    uiView->SetSize((float)internalResolution.x, (float)internalResolution.y);

    uiView->GetRenderer()->UpdateRenderTree();
}

void NoesisRenderPath::ShutdownNoesis() {
    gameStartup.Shutdown();
    cameraSystem.Shutdown();
    caseboardSystem.Shutdown();
    dialogueSystem.Shutdown();

    if (uiView) {
        uiView->GetRenderer()->Shutdown();
    }

    uiView.Reset();
    noesisDevice.Reset();
    rootElement.Reset();
    notificationText.Reset();

    if (frameFence) {
        frameFence->Release();
        frameFence = nullptr;
    }

    Noesis::GUI::Shutdown();
}

void NoesisRenderPath::ShowNotification(const char *message) {
    if (!notificationText)
        return;

    notificationText->SetText(message);
    notificationText->SetVisibility(Noesis::Visibility_Visible);
    notificationText->SetOpacity(1.0f);
    notificationTimer = notificationDuration;
}
