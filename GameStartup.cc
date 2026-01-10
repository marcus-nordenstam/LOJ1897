#include "GameStartup.h"

#include <Systems/animation_system.h>
#include <Systems/character_system.h>

#include <NsGui/Enums.h>

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <fstream>

void GameStartup::Initialize(Noesis::Grid *menu, Noesis::TextBox *seed, Noesis::Button *play,
                             Noesis::Button *fullscreen) {
    menuContainer = Noesis::Ptr<Noesis::Grid>(menu);
    seedTextBox = Noesis::Ptr<Noesis::TextBox>(seed);
    playGameButton = Noesis::Ptr<Noesis::Button>(play);
    fullscreenButton = Noesis::Ptr<Noesis::Button>(fullscreen);

    // Wire up event handlers
    if (playGameButton) {
        playGameButton->Click() +=
            [this](Noesis::BaseComponent *sender, const Noesis::RoutedEventArgs &args) {
                const char *seedText = seedTextBox ? seedTextBox->GetText() : "12345";
                if (playGameCallback) {
                    playGameCallback(seedText ? seedText : "12345");
                }
            };
    }

    if (fullscreenButton) {
        fullscreenButton->Click() +=
            [this](Noesis::BaseComponent *sender, const Noesis::RoutedEventArgs &args) {
                if (fullscreenCallback) {
                    fullscreenCallback();
                }
            };
    }
}

void GameStartup::Shutdown() {
    StopMenuMusic();

    menuContainer.Reset();
    seedTextBox.Reset();
    playGameButton.Reset();
    fullscreenButton.Reset();
}

std::string GameStartup::GetConfigFilePath() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    std::wstring path = exePath;
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        path = path.substr(0, lastSlash + 1);
    }
    path += L"config.ini";

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, NULL, 0, NULL, NULL);
    if (size_needed > 0) {
        std::vector<char> buffer(size_needed);
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, buffer.data(), size_needed, NULL, NULL);
        return std::string(buffer.data());
    }
    return "config.ini";
}

void GameStartup::SaveConfig() {
    std::string configPath = GetConfigFilePath();
    std::ofstream configFile(configPath);
    if (configFile.is_open()) {
        configFile << "project_path = " << projectPath << "\n";
        configFile << "theme_music = " << themeMusic << "\n";
        configFile << "level = " << levelPath << "\n";
        configFile << "player_model = " << playerModel << "\n";
        configFile << "npc_model = " << npcModel << "\n";
        configFile << "anim_lib = " << animLib << "\n";
        configFile << "expression_path = " << expressionPath << "\n";
        configFile.close();

        char buffer[512];
        sprintf_s(buffer, "Saved config: project_path = %s\n", projectPath.c_str());
        wi::backlog::post(buffer);
    }
}

void GameStartup::LoadConfig() {
    std::string configPath = GetConfigFilePath();
    std::ifstream configFile(configPath);
    if (configFile.is_open()) {
        std::string line;
        while (std::getline(configFile, line)) {
            size_t equalsPos = line.find('=');
            if (equalsPos != std::string::npos) {
                std::string key = line.substr(0, equalsPos);
                std::string value = line.substr(equalsPos + 1);

                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);

                // Remove quotes
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
                } else if (key == "anim_lib") {
                    animLib = value;
                } else if (key == "expression_path") {
                    expressionPath = value;
                }
            }
        }
        configFile.close();

        char buffer[512];
        sprintf_s(buffer, "Loaded config:\n  project_path = %s\n  theme_music = %s\n  level = %s\n",
                  projectPath.c_str(), themeMusic.c_str(), levelPath.c_str());
        wi::backlog::post(buffer);
    }

    UpdateControlStates();
}

void GameStartup::InitializeAnimationSystem(wi::scene::Scene &scene) {
    if (projectPath.empty()) {
        wi::backlog::post("Cannot initialize animation system: project_path not set\n");
        return;
    }

    // Load animation library
    if (!animLib.empty()) {
        std::string fullAnimLibPath = projectPath;
        if (!fullAnimLibPath.empty() && fullAnimLibPath.back() != '/' &&
            fullAnimLibPath.back() != '\\') {
            fullAnimLibPath += "\\";
        }
        fullAnimLibPath += animLib;

        for (char &c : fullAnimLibPath) {
            if (c == '/')
                c = '\\';
        }

        char buffer[512];
        sprintf_s(buffer, "Loading animation library: %s\n", fullAnimLibPath.c_str());
        wi::backlog::post(buffer);

        if (wi::helper::FileExists(fullAnimLibPath)) {
            wi::scene::Scene animScene;
            wi::Archive archive(fullAnimLibPath);
            archive.SetSourceDirectory(wi::helper::InferProjectPath(fullAnimLibPath));
            if (archive.IsOpen()) {
                animScene.Serialize(archive);

                animationLibrary.clear();
                for (size_t a = 0; a < animScene.animations.GetCount(); ++a) {
                    wi::ecs::Entity anim_entity = animScene.animations.GetEntity(a);
                    animationLibrary.push_back(anim_entity);

                    const wi::scene::NameComponent *anim_name =
                        animScene.names.GetComponent(anim_entity);
                    sprintf_s(buffer, "Found animation '%s' (entity: %llu)\n",
                              anim_name ? anim_name->name.c_str() : "unnamed", anim_entity);
                    wi::backlog::post(buffer);
                }

                scene.Merge(animScene);

                sprintf_s(buffer, "Animation library loaded with %d animations\n",
                          (int)animationLibrary.size());
                wi::backlog::post(buffer);
            } else {
                sprintf_s(buffer, "ERROR: Failed to open animation library archive: %s\n",
                          fullAnimLibPath.c_str());
                wi::backlog::post(buffer);
            }
        } else {
            sprintf_s(buffer, "ERROR: Animation library not found: %s\n", fullAnimLibPath.c_str());
            wi::backlog::post(buffer);
        }
    } else {
        wi::backlog::post("No animation library (anim_lib) configured in config.ini\n");
    }

    // Load expressions
    if (!expressionPath.empty()) {
        char buffer[512];
        sprintf_s(buffer, "Loading expressions from: %s (project: %s)\n", expressionPath.c_str(),
                  projectPath.c_str());
        wi::backlog::post(buffer);

        scene.LoadExpressions(expressionPath, projectPath);

        wi::backlog::post("Expressions loaded successfully\n");
    } else {
        wi::backlog::post("No expression path (expression_path) configured in config.ini\n");
    }
}

void GameStartup::UpdateControlStates() {
    bool hasProjectPath = !projectPath.empty();

    if (seedTextBox) {
        seedTextBox->SetIsEnabled(hasProjectPath);
    }
    if (playGameButton) {
        playGameButton->SetIsEnabled(hasProjectPath);
    }
    if (fullscreenButton) {
        fullscreenButton->SetIsEnabled(hasProjectPath);
    }

    if (hasProjectPath && !menuMusicInstance.IsValid()) {
        LoadAndPlayMenuMusic();
    } else if (!hasProjectPath && menuMusicInstance.IsValid()) {
        StopMenuMusic();
    }
}

void GameStartup::LoadAndPlayMenuMusic() {
    if (themeMusic.empty()) {
        wi::backlog::post("No theme_music configured in config.ini\n");
        return;
    }

    std::string musicPath = projectPath;
    if (!musicPath.empty() && musicPath.back() != '/' && musicPath.back() != '\\') {
        musicPath += "\\";
    }
    musicPath += themeMusic;

    for (char &c : musicPath) {
        if (c == '/')
            c = '\\';
    }

    if (!wi::helper::FileExists(musicPath)) {
        std::string oggPath = wi::helper::ReplaceExtension(musicPath, "ogg");
        std::string wavPath = wi::helper::ReplaceExtension(musicPath, "wav");

        if (wi::helper::FileExists(oggPath)) {
            musicPath = oggPath;
        } else if (wi::helper::FileExists(wavPath)) {
            musicPath = wavPath;
        } else {
            char buffer[512];
            sprintf_s(buffer, "Music file not found: %s (tried .mp3, .ogg, .wav)\n",
                      musicPath.c_str());
            wi::backlog::post(buffer);
            return;
        }
    }

    if (wi::audio::CreateSound(musicPath, &menuMusic)) {
        menuMusicInstance.type = wi::audio::SUBMIX_TYPE_MUSIC;
        menuMusicInstance.SetLooped(true);

        if (wi::audio::CreateSoundInstance(&menuMusic, &menuMusicInstance)) {
            wi::audio::Play(&menuMusicInstance);

            char buffer[512];
            sprintf_s(buffer, "Started playing menu music: %s\n", musicPath.c_str());
            wi::backlog::post(buffer);
        } else {
            wi::backlog::post("Failed to create music instance\n");
        }
    } else {
        char buffer[512];
        sprintf_s(buffer, "Failed to load music: %s\n", musicPath.c_str());
        wi::backlog::post(buffer);
    }
}

void GameStartup::StopMenuMusic() {
    if (menuMusicInstance.IsValid()) {
        wi::audio::Stop(&menuMusicInstance);
        menuMusicInstance = {};
        menuMusic = {};
    }
}

wi::ecs::Entity GameStartup::FindAnimationByName(const wi::scene::Scene &scene,
                                                 const char *anim_name_substr) {
    for (wi::ecs::Entity lib_anim : animationLibrary) {
        const wi::scene::NameComponent *anim_name = scene.names.GetComponent(lib_anim);
        if (anim_name) {
            std::string name_lower = anim_name->name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            if (name_lower.find(anim_name_substr) != std::string::npos) {
                return lib_anim;
            }
        }
    }
    return wi::ecs::INVALID_ENTITY;
}

void GameStartup::SpawnCharactersFromMetadata(wi::scene::Scene &scene) {
    char buffer[512];

    for (size_t i = 0; i < scene.metadatas.GetCount(); i++) {
        wi::ecs::Entity entity = scene.metadatas.GetEntity(i);
        const wi::scene::MetadataComponent &metadata = scene.metadatas[i];

        wi::scene::TransformComponent *spawnTransform = scene.transforms.GetComponent(entity);
        if (!spawnTransform) {
            continue;
        }

        XMFLOAT3 spawnPos = spawnTransform->GetPosition();
        XMFLOAT3 spawnForward = spawnTransform->GetForward();

        if (metadata.preset == wi::scene::MetadataComponent::Preset::Player) {
            if (playerModel.empty()) {
                sprintf_s(
                    buffer,
                    "Player spawn found at (%.2f, %.2f, %.2f) but no player_model configured\n",
                    spawnPos.x, spawnPos.y, spawnPos.z);
                wi::backlog::post(buffer);
                continue;
            }

            sprintf_s(buffer, "Spawning player at (%.2f, %.2f, %.2f)\n", spawnPos.x, spawnPos.y,
                      spawnPos.z);
            wi::backlog::post(buffer);

            SpawnCharacter(scene, playerModel, spawnPos, spawnForward, true);
        } else if (metadata.preset == wi::scene::MetadataComponent::Preset::NPC) {
            if (npcModel.empty()) {
                sprintf_s(buffer,
                          "NPC spawn found at (%.2f, %.2f, %.2f) but no npc_model configured\n",
                          spawnPos.x, spawnPos.y, spawnPos.z);
                wi::backlog::post(buffer);
                continue;
            }

            sprintf_s(buffer, "Spawning NPC at (%.2f, %.2f, %.2f)\n", spawnPos.x, spawnPos.y,
                      spawnPos.z);
            wi::backlog::post(buffer);

            SpawnCharacter(scene, npcModel, spawnPos, spawnForward, false);
        }
    }
}

wi::ecs::Entity GameStartup::SpawnCharacter(wi::scene::Scene &scene, const std::string &modelPath,
                                            const XMFLOAT3 &position, const XMFLOAT3 &forward,
                                            bool isPlayer) {
    std::string fullModelPath = projectPath;
    if (!fullModelPath.empty() && fullModelPath.back() != '/' && fullModelPath.back() != '\\') {
        fullModelPath += "\\";
    }
    fullModelPath += modelPath;

    for (char &c : fullModelPath) {
        if (c == '/')
            c = '\\';
    }

    char buffer[512];
    sprintf_s(buffer, "Loading character model: %s\n", fullModelPath.c_str());
    wi::backlog::post(buffer);

    if (!wi::helper::FileExists(fullModelPath)) {
        sprintf_s(buffer, "ERROR: Character model not found: %s\n", fullModelPath.c_str());
        wi::backlog::post(buffer);
        return wi::ecs::INVALID_ENTITY;
    }

    XMMATRIX spawnMatrix = XMMatrixTranslation(position.x, position.y, position.z);

    wi::scene::Scene tempScene;
    wi::ecs::Entity modelRoot = wi::scene::LoadModel(tempScene, fullModelPath, spawnMatrix, true);

    if (modelRoot == wi::ecs::INVALID_ENTITY) {
        sprintf_s(buffer, "ERROR: Failed to load character model: %s\n", fullModelPath.c_str());
        wi::backlog::post(buffer);
        return wi::ecs::INVALID_ENTITY;
    }

    wi::ecs::Entity humanoidEntity = wi::ecs::INVALID_ENTITY;
    for (size_t h = 0; h < tempScene.cc_humanoids.GetCount(); ++h) {
        wi::ecs::Entity hEntity = tempScene.cc_humanoids.GetEntity(h);
        if (hEntity == modelRoot || tempScene.Entity_IsDescendant(hEntity, modelRoot)) {
            humanoidEntity = hEntity;
            break;
        }
    }

    scene.Merge(tempScene);
    scene.ResetPose(modelRoot);

    wi::ecs::Entity characterEntity = modelRoot;

    wi::scene::CharacterComponent &character = scene.characters.Create(characterEntity);
    character.SetPosition(position);
    character.SetFacing(forward);
    character.SetFootPlacementEnabled(true);
    character.width = 0.3f;
    character.height = 1.86f;
    character.turning_speed = 9.0f;
    character.gravity = -30.0f;
    character.fixed_update_fps = 120.0f;
    character.ground_friction = 0.92f;
    character.slope_threshold = 0.2f;

    wi::scene::MindComponent &mind = scene.minds.Create(characterEntity);

    wi::scene::TransformComponent *transform = scene.transforms.GetComponent(characterEntity);
    if (transform == nullptr) {
        transform = &scene.transforms.Create(characterEntity);
        transform->ClearTransform();
        transform->Translate(position);
        transform->UpdateTransform();
    }

    wi::scene::LayerComponent *layer = scene.layers.GetComponent(characterEntity);
    if (layer == nullptr) {
        layer = &scene.layers.Create(characterEntity);
    }

    if (isPlayer) {
        mind.type = wi::scene::MindComponent::Type::Player;
        layer->layerMask = 1 << 0;
        playerCharacter = characterEntity;
    } else {
        mind.type = wi::scene::MindComponent::Type::NPC;
        layer->layerMask = 1 << 1;

        bool is_patrol = (rand() % 2) == 0;
        if (is_patrol) {
            mind.scriptCallback = "npc_patrol_update";
        } else {
            mind.scriptCallback = "npc_guard_update";
        }

        npcEntities.push_back(characterEntity);
    }

    // Retarget animations
    if (modelRoot != wi::ecs::INVALID_ENTITY && !animationLibrary.empty()) {
        sprintf_s(buffer, "Retargeting animations for character (Entity: %llu)...\n",
                  characterEntity);
        wi::backlog::post(buffer);

        wi::ecs::Entity idle_anim = FindAnimationByName(scene, "idle");
        wi::ecs::Entity walk_anim = FindAnimationByName(scene, "walk");
        wi::ecs::Entity sit_anim = FindAnimationByName(scene, "sit");

        if (idle_anim != wi::ecs::INVALID_ENTITY) {
            character.action_anim_entities[(int)wi::scene::ActionVerb::Idle] =
                wi::scene::animation_system::retarget(scene, idle_anim, modelRoot);
        }
        if (walk_anim != wi::ecs::INVALID_ENTITY) {
            character.action_anim_entities[(int)wi::scene::ActionVerb::Walk] =
                wi::scene::animation_system::retarget(scene, walk_anim, modelRoot);
            character.action_anim_entities[(int)wi::scene::ActionVerb::WalkTo] =
                character.action_anim_entities[(int)wi::scene::ActionVerb::Walk];
        }
        if (sit_anim != wi::ecs::INVALID_ENTITY) {
            character.action_anim_entities[(int)wi::scene::ActionVerb::Sit] =
                wi::scene::animation_system::retarget(scene, sit_anim, modelRoot);
            character.action_anim_entities[(int)wi::scene::ActionVerb::Stand] =
                character.action_anim_entities[(int)wi::scene::ActionVerb::Sit];
        }

        character.SetAction(scene, wi::scene::character_system::make_idle(scene, character));

        wi::backlog::post("Animations retargeted successfully\n");
    }

    // Bind expressions
    if (modelRoot != wi::ecs::INVALID_ENTITY && !scene.unbound_expressions.empty()) {
        sprintf_s(buffer, "Binding expressions for character (Entity: %llu)...\n", characterEntity);
        wi::backlog::post(buffer);

        int mapped_count = 0;
        for (size_t expr_index = 0; expr_index < scene.unbound_expressions.size(); expr_index++) {
            const auto &expr = scene.unbound_expressions[expr_index];
            auto morph_entities = scene.find_entities_with_morphs(modelRoot, expr);
            for (auto target_entity : morph_entities) {
                auto expr_comp = scene.demand_expression_component(target_entity);
                if (expr_comp->bind_expression(scene, expr_index, target_entity)) {
                    mapped_count++;
                    sprintf_s(buffer, "Mapped expression '%s' to entity %llu\n", expr.name.c_str(),
                              target_entity);
                    wi::backlog::post(buffer);
                }
            }
        }
        sprintf_s(buffer, "Bound %d of %d expressions\n", mapped_count,
                  (int)scene.unbound_expressions.size());
        wi::backlog::post(buffer);
    }

    sprintf_s(buffer, "%s character spawned successfully (Entity: %llu)\n",
              isPlayer ? "Player" : "NPC", characterEntity);
    wi::backlog::post(buffer);

    return characterEntity;
}

void GameStartup::LoadNPCScripts() {
    std::string basePath = projectPath;
    if (!basePath.empty() && basePath.back() != '/' && basePath.back() != '\\') {
        basePath += "/";
    }

    if (!patrolScriptLoaded) {
        std::vector<std::string> script_paths = {basePath + "Content/scripts/npc/patrol.lua",
                                                 basePath + "SharedContent/scripts/npc/patrol.lua",
                                                 basePath + "scripts/npc/patrol.lua"};

        for (const auto &patrol_script_path : script_paths) {
            if (wi::helper::FileExists(patrol_script_path)) {
                wi::lua::RunFile(patrol_script_path);
                patrolScriptLoaded = true;
                char buffer[512];
                sprintf_s(buffer, "Loaded NPC patrol script from: %s\n",
                          patrol_script_path.c_str());
                wi::backlog::post(buffer);
                break;
            }
        }

        if (!patrolScriptLoaded) {
            wi::backlog::post("WARNING: NPC patrol script not found in any location\n");
        }
    }

    if (!guardScriptLoaded) {
        std::vector<std::string> script_paths = {basePath + "Content/scripts/npc/guard.lua",
                                                 basePath + "SharedContent/scripts/npc/guard.lua",
                                                 basePath + "scripts/npc/guard.lua"};

        for (const auto &guard_script_path : script_paths) {
            if (wi::helper::FileExists(guard_script_path)) {
                wi::lua::RunFile(guard_script_path);
                guardScriptLoaded = true;
                char buffer[512];
                sprintf_s(buffer, "Loaded NPC guard script from: %s\n", guard_script_path.c_str());
                wi::backlog::post(buffer);
                break;
            }
        }

        if (!guardScriptLoaded) {
            wi::backlog::post("WARNING: NPC guard script not found in any location\n");
        }
    }
}

void GameStartup::CleanupNPCScripts() {
    if (patrolScriptLoaded) {
        wi::lua::RunText("npc_patrol_clear_all()");
        patrolScriptLoaded = false;
    }
    if (guardScriptLoaded) {
        wi::lua::RunText("npc_guard_clear_all()");
        guardScriptLoaded = false;
    }
    npcEntities.clear();
}

void GameStartup::LoadGameScene(wi::scene::Scene &scene) {
    auto startTime = std::chrono::high_resolution_clock::now();

    if (levelPath.empty()) {
        wi::backlog::post("ERROR: No level path configured in config.ini\n");
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        char buffer[512];
        sprintf_s(buffer, "LoadGameScene aborted (no level path) in %lld ms\n", duration.count());
        wi::backlog::post(buffer);
        return;
    }

    std::string scenePath = projectPath;
    if (!scenePath.empty() && scenePath.back() != '/' && scenePath.back() != '\\') {
        scenePath += "\\";
    }
    scenePath += levelPath;

    for (char &c : scenePath) {
        if (c == '/')
            c = '\\';
    }

    char buffer[512];
    sprintf_s(buffer, "Loading scene: %s\n", scenePath.c_str());
    wi::backlog::post(buffer);

    if (!wi::helper::FileExists(scenePath)) {
        sprintf_s(buffer, "ERROR: Scene file not found: %s\n", scenePath.c_str());
        wi::backlog::post(buffer);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        sprintf_s(buffer, "LoadGameScene aborted (file not found) in %lld ms\n", duration.count());
        wi::backlog::post(buffer);
        return;
    }

    scene.Clear();

    wi::Archive archive(scenePath);
    archive.SetSourceDirectory(wi::helper::InferProjectPath(scenePath));
    if (archive.IsOpen()) {
        scene.Serialize(archive);

        wi::backlog::post("Scene loaded successfully\n");

        InitializeAnimationSystem(scene);

        scene.Update(0.0f);

        SpawnCharactersFromMetadata(scene);

        if (!npcEntities.empty()) {
            LoadNPCScripts();
        }

    } else {
        sprintf_s(buffer, "ERROR: Failed to open scene archive: %s\n", scenePath.c_str());
        wi::backlog::post(buffer);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    sprintf_s(buffer, "LoadGameScene completed in %lld ms (%.2f seconds)\n", duration.count(),
              duration.count() / 1000.0f);
    wi::backlog::post(buffer);
}

void GameStartup::ToggleFullscreen(HWND windowHandle) {
    if (!windowHandle)
        return;

    if (!isFullscreen) {
        windowedStyle = GetWindowLong(windowHandle, GWL_STYLE);
        windowedExStyle = GetWindowLong(windowHandle, GWL_EXSTYLE);
        GetWindowRect(windowHandle, &windowedRect);

        MONITORINFO mi = {sizeof(mi)};
        if (GetMonitorInfo(MonitorFromWindow(windowHandle, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(windowHandle, GWL_STYLE, windowedStyle & ~(WS_CAPTION | WS_THICKFRAME));
            SetWindowLong(windowHandle, GWL_EXSTYLE,
                          windowedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                                              WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

            SetWindowPos(windowHandle, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

            isFullscreen = true;

            if (fullscreenButton) {
                fullscreenButton->SetContent("WINDOWED");
            }
        }
    } else {
        SetWindowLong(windowHandle, GWL_STYLE, windowedStyle);
        SetWindowLong(windowHandle, GWL_EXSTYLE, windowedExStyle);

        SetWindowPos(windowHandle, HWND_NOTOPMOST, windowedRect.left, windowedRect.top,
                     windowedRect.right - windowedRect.left, windowedRect.bottom - windowedRect.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        isFullscreen = false;

        if (fullscreenButton) {
            fullscreenButton->SetContent("FULLSCREEN");
        }
    }
}

void GameStartup::SetFullscreenState(bool fullscreen) {
    isFullscreen = fullscreen;
    if (fullscreenButton) {
        fullscreenButton->SetContent(fullscreen ? "WINDOWED" : "FULLSCREEN");
    }
}

void GameStartup::ShowMenu(bool visible) {
    if (menuContainer) {
        menuContainer->SetVisibility(visible ? Noesis::Visibility_Visible
                                             : Noesis::Visibility_Collapsed);
    }
}
