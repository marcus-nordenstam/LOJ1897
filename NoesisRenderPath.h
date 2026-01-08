#pragma once

#include <Windows.h>

#include "WickedEngine.h"
#include <wiAudio.h>
#include <wiGraphicsDevice_DX12.h>

#include <Systems/animation_system.h>
#include <Systems/character_system.h>

#include <NsApp/LocalFontProvider.h>
#include <NsApp/LocalTextureProvider.h>
#include <NsApp/LocalXamlProvider.h>
#include <NsCore/Nullable.h>
#include <NsCore/ReflectionImplement.h>
#include <NsDrawing/Color.h>
#include <NsGui/Button.h>
#include <NsGui/Grid.h>
#include <NsGui/IRenderer.h>
#include <NsGui/IView.h>
#include <NsGui/InputEnums.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/SolidColorBrush.h>
#include <NsGui/TextBox.h>
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
    Noesis::Ptr<Noesis::FrameworkElement> talkIndicator; // Talk indicator (shown when aiming at NPC)
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
    bool menuVisible = true;

    // Player character tracking
    wi::ecs::Entity playerCharacter = wi::ecs::INVALID_ENTITY;
    float cameraHorizontal = 0.0f;        // Camera yaw angle
    float cameraVertical = 0.3f;          // Camera pitch angle
    float cameraDistance = 2.5f;          // Camera distance from player
    float cameraHorizontalOffset = 0.25f; // Over-the-shoulder horizontal offset (positive = right)

    // NPC tracking and Lua scripts
    std::vector<wi::ecs::Entity> npcEntities;
    bool patrolScriptLoaded = false;
    bool guardScriptLoaded = false;

    // Aim dot (reticle) - raycast from character's eye in camera direction
    bool aimDotVisible = false;
    XMFLOAT2 aimDotScreenPos = {0, 0};
    bool aimingAtNPC = false; // True if aim ray hits an NPC

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
    bool IsMenuVisible() const { return menuVisible; }

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
            WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, buffer.data(), size_needed, NULL,
                                NULL);
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
            configFile << "anim_lib = " << animLib << "\n";
            configFile << "expression_path = " << expressionPath << "\n";
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
                    } else if (key == "anim_lib") {
                        animLib = value;
                    } else if (key == "expression_path") {
                        expressionPath = value;
                    }
                }
            }
            configFile.close();

            char buffer[512];
            sprintf_s(buffer,
                      "Loaded config:\n  project_path = %s\n  theme_music = %s\n  level = %s\n",
                      projectPath.c_str(), themeMusic.c_str(), levelPath.c_str());
            OutputDebugStringA(buffer);
        }

        // Update control states based on project path
        UpdateControlStates();
    }

    void InitializeAnimationSystem() {
        if (projectPath.empty()) {
            OutputDebugStringA("Cannot initialize animation system: project_path not set\n");
            return;
        }

        wi::scene::Scene &scene = wi::scene::GetScene();

        // Load animation library if specified
        if (!animLib.empty()) {
            std::string fullAnimLibPath = projectPath;
            if (!fullAnimLibPath.empty() && fullAnimLibPath.back() != '/' &&
                fullAnimLibPath.back() != '\\') {
                fullAnimLibPath += "\\";
            }
            fullAnimLibPath += animLib;

            // Normalize path separators
            for (char &c : fullAnimLibPath) {
                if (c == '/')
                    c = '\\';
            }

            char buffer[512];
            sprintf_s(buffer, "Loading animation library: %s\n", fullAnimLibPath.c_str());
            OutputDebugStringA(buffer);

            if (wi::helper::FileExists(fullAnimLibPath)) {
                // Load animation library scene
                wi::scene::Scene animScene;
                wi::Archive archive(fullAnimLibPath);
                archive.SetSourceDirectory(wi::helper::InferProjectPath(fullAnimLibPath));
                if (archive.IsOpen()) {
                    animScene.Serialize(archive);

                    // Store animation entities for retargeting
                    animationLibrary.clear();
                    for (size_t a = 0; a < animScene.animations.GetCount(); ++a) {
                        wi::ecs::Entity anim_entity = animScene.animations.GetEntity(a);
                        animationLibrary.push_back(anim_entity);

                        const wi::scene::NameComponent *anim_name =
                            animScene.names.GetComponent(anim_entity);
                        sprintf_s(buffer, "Found animation '%s' (entity: %llu)\n",
                                  anim_name ? anim_name->name.c_str() : "unnamed", anim_entity);
                        OutputDebugStringA(buffer);
                    }

                    // Merge animation library into main scene
                    scene.Merge(animScene);

                    sprintf_s(buffer, "Animation library loaded with %d animations\n",
                              (int)animationLibrary.size());
                    OutputDebugStringA(buffer);
                } else {
                    sprintf_s(buffer, "ERROR: Failed to open animation library archive: %s\n",
                              fullAnimLibPath.c_str());
                    OutputDebugStringA(buffer);
                }
            } else {
                sprintf_s(buffer, "ERROR: Animation library not found: %s\n",
                          fullAnimLibPath.c_str());
                OutputDebugStringA(buffer);
            }
        } else {
            OutputDebugStringA("No animation library (anim_lib) configured in config.ini\n");
        }

        // Load expressions (required by action system, same as walkabout mode)
        if (!expressionPath.empty()) {
            char buffer[512];
            sprintf_s(buffer, "Loading expressions from: %s (project: %s)\n",
                      expressionPath.c_str(), projectPath.c_str());
            OutputDebugStringA(buffer);

            // Use the same method as walkabout mode
            scene.LoadExpressions(expressionPath, projectPath);

            OutputDebugStringA("Expressions loaded successfully\n");
        } else {
            OutputDebugStringA("No expression path (expression_path) configured in config.ini\n");
        }
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
        for (char &c : musicPath) {
            if (c == '/')
                c = '\\';
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
                sprintf_s(buffer, "Music file not found: %s (tried .mp3, .ogg, .wav)\n",
                          musicPath.c_str());
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

    // Helper to find animation entity by name substring (like walkabout)
    wi::ecs::Entity FindAnimationByName(const wi::scene::Scene &scene,
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

    void SpawnCharactersFromMetadata(wi::scene::Scene &scene) {
        char buffer[512];

        // Find all entities with metadata components
        for (size_t i = 0; i < scene.metadatas.GetCount(); i++) {
            wi::ecs::Entity entity = scene.metadatas.GetEntity(i);
            const wi::scene::MetadataComponent &metadata = scene.metadatas[i];

            // Get the transform of the spawn point
            wi::scene::TransformComponent *spawnTransform = scene.transforms.GetComponent(entity);
            if (!spawnTransform) {
                continue; // Need a transform to know where to spawn
            }

            XMFLOAT3 spawnPos = spawnTransform->GetPosition();
            XMFLOAT3 spawnForward = spawnTransform->GetForward();

            // Check if this is a Player spawn point
            if (metadata.preset == wi::scene::MetadataComponent::Preset::Player) {
                if (playerModel.empty()) {
                    sprintf_s(
                        buffer,
                        "Player spawn found at (%.2f, %.2f, %.2f) but no player_model configured\n",
                        spawnPos.x, spawnPos.y, spawnPos.z);
                    OutputDebugStringA(buffer);
                    continue;
                }

                sprintf_s(buffer, "Spawning player at (%.2f, %.2f, %.2f)\n", spawnPos.x, spawnPos.y,
                          spawnPos.z);
                OutputDebugStringA(buffer);

                SpawnCharacter(scene, playerModel, spawnPos, spawnForward, true);
            }
            // Check if this is an NPC spawn point
            else if (metadata.preset == wi::scene::MetadataComponent::Preset::NPC) {
                if (npcModel.empty()) {
                    sprintf_s(buffer,
                              "NPC spawn found at (%.2f, %.2f, %.2f) but no npc_model configured\n",
                              spawnPos.x, spawnPos.y, spawnPos.z);
                    OutputDebugStringA(buffer);
                    continue;
                }

                sprintf_s(buffer, "Spawning NPC at (%.2f, %.2f, %.2f)\n", spawnPos.x, spawnPos.y,
                          spawnPos.z);
                OutputDebugStringA(buffer);

                SpawnCharacter(scene, npcModel, spawnPos, spawnForward, false);
            }
        }
    }

    wi::ecs::Entity SpawnCharacter(wi::scene::Scene &scene, const std::string &modelPath,
                                   const XMFLOAT3 &position, const XMFLOAT3 &forward,
                                   bool isPlayer) {
        // Construct full model path
        std::string fullModelPath = projectPath;
        if (!fullModelPath.empty() && fullModelPath.back() != '/' && fullModelPath.back() != '\\') {
            fullModelPath += "\\";
        }
        fullModelPath += modelPath;

        // Normalize path separators
        for (char &c : fullModelPath) {
            if (c == '/')
                c = '\\';
        }

        char buffer[512];
        sprintf_s(buffer, "Loading character model: %s\n", fullModelPath.c_str());
        OutputDebugStringA(buffer);

        // Check if file exists
        if (!wi::helper::FileExists(fullModelPath)) {
            sprintf_s(buffer, "ERROR: Character model not found: %s\n", fullModelPath.c_str());
            OutputDebugStringA(buffer);
            return wi::ecs::INVALID_ENTITY;
        }

        // Create spawn matrix
        XMMATRIX spawnMatrix = XMMatrixTranslation(position.x, position.y, position.z);

        // Load model into temporary scene
        wi::scene::Scene tempScene;
        wi::ecs::Entity modelRoot =
            wi::scene::LoadModel(tempScene, fullModelPath, spawnMatrix, true);

        if (modelRoot == wi::ecs::INVALID_ENTITY) {
            sprintf_s(buffer, "ERROR: Failed to load character model: %s\n", fullModelPath.c_str());
            OutputDebugStringA(buffer);
            return wi::ecs::INVALID_ENTITY;
        }

        // Find humanoid component in loaded model
        wi::ecs::Entity humanoidEntity = wi::ecs::INVALID_ENTITY;
        for (size_t h = 0; h < tempScene.cc_humanoids.GetCount(); ++h) {
            wi::ecs::Entity hEntity = tempScene.cc_humanoids.GetEntity(h);
            if (hEntity == modelRoot || tempScene.Entity_IsDescendant(hEntity, modelRoot)) {
                humanoidEntity = hEntity;
                break;
            }
        }

        // Merge into main scene
        scene.Merge(tempScene);
        scene.ResetPose(modelRoot);

        // Create character entity (use model root)
        wi::ecs::Entity characterEntity = modelRoot;

        // Create CharacterComponent
        wi::scene::CharacterComponent &character = scene.characters.Create(characterEntity);
        character.SetPosition(position);
        character.SetFacing(forward);
        character.SetFootPlacementEnabled(true);
        character.width = 0.3f;   // 30cm radius capsule
        character.height = 1.86f; // 186cm tall
        character.turning_speed = 9.0f;
        character.gravity = -30.0f;
        character.fixed_update_fps = 120.0f;
        character.ground_friction = 0.92f;
        character.slope_threshold = 0.2f;

        // Create MindComponent for action system (like walkabout mode)
        wi::scene::MindComponent &mind = scene.minds.Create(characterEntity);

        // Create transform component if not already present
        wi::scene::TransformComponent *transform = scene.transforms.GetComponent(characterEntity);
        if (transform == nullptr) {
            transform = &scene.transforms.Create(characterEntity);
            transform->ClearTransform();
            transform->Translate(position);
            transform->UpdateTransform();
        }

        // Set layer for collision filtering
        wi::scene::LayerComponent *layer = scene.layers.GetComponent(characterEntity);
        if (layer == nullptr) {
            layer = &scene.layers.Create(characterEntity);
        }

        if (isPlayer) {
            // Player-specific setup
            mind.type = wi::scene::MindComponent::Type::Player;
            layer->layerMask = 1 << 0; // Player layer

            // Store player character reference for camera following
            playerCharacter = characterEntity;
        } else {
            // NPC-specific setup
            mind.type = wi::scene::MindComponent::Type::NPC;
            layer->layerMask = 1 << 1; // NPC layer

            // Assign script callback (50% patrol, 50% guard like walkabout)
            bool is_patrol = (rand() % 2) == 0;
            if (is_patrol) {
                mind.scriptCallback = "npc_patrol_update";
            } else {
                mind.scriptCallback = "npc_guard_update";
            }

            // Track NPC entity for Lua updates
            npcEntities.push_back(characterEntity);
        }

        // === RETARGET ANIMATIONS (like walkabout) ===
        if (modelRoot != wi::ecs::INVALID_ENTITY && !animationLibrary.empty()) {
            sprintf_s(buffer, "Retargeting animations for character (Entity: %llu)...\n",
                      characterEntity);
            OutputDebugStringA(buffer);

            // Find required animations from library
            wi::ecs::Entity idle_anim = FindAnimationByName(scene, "idle");
            wi::ecs::Entity walk_anim = FindAnimationByName(scene, "walk");
            wi::ecs::Entity sit_anim = FindAnimationByName(scene, "sit");

            // Retarget and assign to character's action_anim_entities
            if (idle_anim != wi::ecs::INVALID_ENTITY) {
                character.action_anim_entities[(int)wi::scene::ActionVerb::Idle] =
                    wi::scene::animation_system::retarget(scene, idle_anim, modelRoot);
            }
            if (walk_anim != wi::ecs::INVALID_ENTITY) {
                character.action_anim_entities[(int)wi::scene::ActionVerb::Walk] =
                    wi::scene::animation_system::retarget(scene, walk_anim, modelRoot);
                // WalkTo uses same animation as Walk
                character.action_anim_entities[(int)wi::scene::ActionVerb::WalkTo] =
                    character.action_anim_entities[(int)wi::scene::ActionVerb::Walk];
            }
            if (sit_anim != wi::ecs::INVALID_ENTITY) {
                character.action_anim_entities[(int)wi::scene::ActionVerb::Sit] =
                    wi::scene::animation_system::retarget(scene, sit_anim, modelRoot);
                // Stand is sit played backwards
                character.action_anim_entities[(int)wi::scene::ActionVerb::Stand] =
                    character.action_anim_entities[(int)wi::scene::ActionVerb::Sit];
            }

            // Play idle animation by default
            character.SetAction(scene, wi::scene::character_system::make_idle(scene, character));

            OutputDebugStringA("Animations retargeted successfully\n");
        }

        // === BIND EXPRESSIONS (like walkabout) ===
        if (modelRoot != wi::ecs::INVALID_ENTITY && !scene.unbound_expressions.empty()) {
            sprintf_s(buffer, "Binding expressions for character (Entity: %llu)...\n",
                      characterEntity);
            OutputDebugStringA(buffer);

            int mapped_count = 0;
            for (size_t expr_index = 0; expr_index < scene.unbound_expressions.size();
                 expr_index++) {
                const auto &expr = scene.unbound_expressions[expr_index];
                auto morph_entities = scene.find_entities_with_morphs(modelRoot, expr);
                for (auto target_entity : morph_entities) {
                    auto expr_comp = scene.demand_expression_component(target_entity);
                    if (expr_comp->bind_expression(scene, expr_index, target_entity)) {
                        mapped_count++;
                        sprintf_s(buffer, "Mapped expression '%s' to entity %llu\n",
                                  expr.name.c_str(), target_entity);
                        OutputDebugStringA(buffer);
                    }
                }
            }
            sprintf_s(buffer, "Bound %d of %d expressions\n", mapped_count,
                      (int)scene.unbound_expressions.size());
            OutputDebugStringA(buffer);
        }

        sprintf_s(buffer, "%s character spawned successfully (Entity: %llu)\n",
                  isPlayer ? "Player" : "NPC", characterEntity);
        OutputDebugStringA(buffer);

        return characterEntity;
    }

    void LoadNPCScripts() {
        // Ensure project path ends with separator
        std::string basePath = projectPath;
        if (!basePath.empty() && basePath.back() != '/' && basePath.back() != '\\') {
            basePath += "/";
        }

        // Try multiple locations for the patrol script (like walkabout)
        if (!patrolScriptLoaded) {
            std::vector<std::string> script_paths = {basePath + "Content/scripts/npc/patrol.lua",
                                                     basePath +
                                                         "SharedContent/scripts/npc/patrol.lua",
                                                     basePath + "scripts/npc/patrol.lua"};

            for (const auto &patrol_script_path : script_paths) {
                if (wi::helper::FileExists(patrol_script_path)) {
                    wi::lua::RunFile(patrol_script_path);
                    patrolScriptLoaded = true;
                    char buffer[512];
                    sprintf_s(buffer, "Loaded NPC patrol script from: %s\n",
                              patrol_script_path.c_str());
                    OutputDebugStringA(buffer);
                    break;
                }
            }

            if (!patrolScriptLoaded) {
                OutputDebugStringA("WARNING: NPC patrol script not found in any location\n");
            }
        }

        // Try multiple locations for the guard script (like walkabout)
        if (!guardScriptLoaded) {
            std::vector<std::string> script_paths = {basePath + "Content/scripts/npc/guard.lua",
                                                     basePath +
                                                         "SharedContent/scripts/npc/guard.lua",
                                                     basePath + "scripts/npc/guard.lua"};

            for (const auto &guard_script_path : script_paths) {
                if (wi::helper::FileExists(guard_script_path)) {
                    wi::lua::RunFile(guard_script_path);
                    guardScriptLoaded = true;
                    char buffer[512];
                    sprintf_s(buffer, "Loaded NPC guard script from: %s\n",
                              guard_script_path.c_str());
                    OutputDebugStringA(buffer);
                    break;
                }
            }

            if (!guardScriptLoaded) {
                OutputDebugStringA("WARNING: NPC guard script not found in any location\n");
            }
        }
    }

    void CleanupNPCScripts() {
        // Clean up Lua NPC script state (like walkabout)
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
        for (char &c : scenePath) {
            if (c == '/')
                c = '\\';
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
        wi::scene::Scene &scene = wi::scene::GetScene();

        // Clear existing scene
        scene.Clear();

        // Load the new scene
        wi::Archive archive(scenePath);
        archive.SetSourceDirectory(wi::helper::InferProjectPath(scenePath));
        if (archive.IsOpen()) {
            scene.Serialize(archive);

            OutputDebugStringA("Scene loaded successfully\n");

            // Load animation library and expressions AFTER loading scene
            // (scene.Clear() would have wiped them if loaded earlier)
            InitializeAnimationSystem();

            // Update scene to compute bounds
            scene.Update(0.0f);

            // Spawn player and NPCs based on metadata components
            SpawnCharactersFromMetadata(scene);

            // Load NPC Lua scripts if NPCs were spawned (like walkabout)
            if (!npcEntities.empty()) {
                LoadNPCScripts();
            }

            // Initialize camera angles if player was spawned
            if (playerCharacter != wi::ecs::INVALID_ENTITY) {
                wi::scene::CharacterComponent *playerChar =
                    scene.characters.GetComponent(playerCharacter);
                if (playerChar) {
                    XMFLOAT3 playerForward = playerChar->GetFacing();
                    cameraHorizontal = atan2f(playerForward.x, playerForward.z);
                    cameraVertical = 0.3f;
                    cameraDistance = 2.5f;

                    OutputDebugStringA("Camera initialized to follow player\n");
                }
            } else {
                OutputDebugStringA(
                    "WARNING: No player character spawned, camera will not follow\n");
            }

        } else {
            sprintf_s(buffer, "ERROR: Failed to open scene archive: %s\n", scenePath.c_str());
            OutputDebugStringA(buffer);
        }
    }

    // Enable/disable walkabout control mode (mouse capture for third-person camera control)
    void SetFirstPersonMode(bool enabled) {
        if (!windowHandle)
            return;

        isFirstPersonMode = enabled;

        if (enabled) {
            // Hide and capture the cursor (like editor walkabout mode)
            ShowCursor(FALSE);

            // Get window center for mouse reset
            RECT clientRect;
            GetClientRect(windowHandle, &clientRect);
            POINT center = {(clientRect.right - clientRect.left) / 2,
                            (clientRect.bottom - clientRect.top) / 2};
            ClientToScreen(windowHandle, &center);
            SetCursorPos(center.x, center.y);

            // Clip cursor to window
            RECT windowRect;
            GetWindowRect(windowHandle, &windowRect);
            ClipCursor(&windowRect);

            mouseInitialized = false;

            char buffer[256];
            sprintf_s(buffer,
                      "Walkabout control mode enabled. Mouse captured for third-person camera.\n");
            OutputDebugStringA(buffer);
        } else {
            // Show cursor and release capture
            ShowCursor(TRUE);
            ClipCursor(nullptr);

            mouseInitialized = false;

            OutputDebugStringA("Walkabout control mode disabled. Mouse released.\n");
        }
    }

    // Toggle fullscreen mode
    void ToggleFullscreen() {
        if (!windowHandle)
            return;

        if (!isFullscreen) {
            // Save current window state
            windowedStyle = GetWindowLong(windowHandle, GWL_STYLE);
            windowedExStyle = GetWindowLong(windowHandle, GWL_EXSTYLE);
            GetWindowRect(windowHandle, &windowedRect);

            // Get monitor info
            MONITORINFO mi = {sizeof(mi)};
            if (GetMonitorInfo(MonitorFromWindow(windowHandle, MONITOR_DEFAULTTOPRIMARY), &mi)) {
                // Remove window decorations and set fullscreen style
                SetWindowLong(windowHandle, GWL_STYLE,
                              windowedStyle & ~(WS_CAPTION | WS_THICKFRAME));
                SetWindowLong(windowHandle, GWL_EXSTYLE,
                              windowedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                                                  WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

                // Set window to cover entire monitor
                SetWindowPos(windowHandle, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
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

            SetWindowPos(windowHandle, HWND_NOTOPMOST, windowedRect.left, windowedRect.top,
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
        // Note: Animation library is loaded in LoadGameScene() after scene.Clear()
    }

    void Stop() override {
        // Cleanup Noesis
        ShutdownNoesis();

        // Call parent Stop
        RenderPath3D::Stop();
    }

    void Update(float dt) override {
        // Call parent Update first
        RenderPath3D::Update(dt);

        // Handle walkabout-style controls and third-person camera when game is active
        if (!menuVisible && playerCharacter != wi::ecs::INVALID_ENTITY) {
            wi::scene::Scene &scene = wi::scene::GetScene();
            wi::scene::CharacterComponent *playerChar =
                scene.characters.GetComponent(playerCharacter);

            if (playerChar && isFirstPersonMode) {
                // ===== MOUSE LOOK (Action System) =====
                if (windowHandle) {
                    // Get window center
                    RECT clientRect;
                    GetClientRect(windowHandle, &clientRect);
                    POINT center = {(clientRect.right - clientRect.left) / 2,
                                    (clientRect.bottom - clientRect.top) / 2};

                    // Get current cursor position
                    POINT currentPos;
                    GetCursorPos(&currentPos);
                    ScreenToClient(windowHandle, &currentPos);

                    if (mouseInitialized) {
                        // Calculate mouse delta
                        float deltaX = (float)(currentPos.x - center.x) * mouseSensitivity;
                        float deltaY = (float)(currentPos.y - center.y) * mouseSensitivity;

                        if (deltaX != 0.0f || deltaY != 0.0f) {
                            // Apply mouse look to camera angles
                            cameraHorizontal += deltaX;
                            cameraVertical =
                                std::clamp(cameraVertical + deltaY, -XM_PIDIV4, XM_PIDIV4 * 1.2f);
                        }
                    } else {
                        mouseInitialized = true;
                    }

                    // Reset cursor to center for continuous movement
                    POINT screenCenter = center;
                    ClientToScreen(windowHandle, &screenCenter);
                    SetCursorPos(screenCenter.x, screenCenter.y);
                }

                // Camera zoom with mouse wheel
                XMFLOAT4 pointer = wi::input::GetPointer();
                cameraDistance = std::max(0.5f, cameraDistance - pointer.z * 0.3f);

                // === ACTION SYSTEM: BODY MOTOR - WALK ===
                bool wPressed = (GetAsyncKeyState('W') & 0x8000) != 0;
                bool sPressed = (GetAsyncKeyState('S') & 0x8000) != 0;

                if (wPressed || sPressed) {
                    if (playerChar->IsGrounded() || playerChar->IsWallIntersect()) {
                        // Calculate walk direction based on camera and input
                        XMFLOAT3 walk_dir;
                        if (sPressed && !wPressed) {
                            // S key: walk toward camera (backward)
                            walk_dir.x = -sinf(cameraHorizontal);
                            walk_dir.y = 0.0f;
                            walk_dir.z = -cosf(cameraHorizontal);
                        } else {
                            // W key: walk away from camera (forward)
                            walk_dir.x = sinf(cameraHorizontal);
                            walk_dir.y = 0.0f;
                            walk_dir.z = cosf(cameraHorizontal);
                        }

                        // Walk action with direction (handles body turning internally)
                        auto action =
                            wi::scene::character_system::make_walk(scene, *playerChar, walk_dir);
                        playerChar->SetAction(scene, action);
                    }
                } else if (playerChar->IsWalking()) {
                    // Return to idle when no movement key pressed
                    auto action = wi::scene::character_system::make_idle(scene, *playerChar);
                    playerChar->SetAction(scene, action);
                }

                // === ACTION SYSTEM: BODY MOTOR - JUMP ===
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
                    SetFirstPersonMode(false);
                    menuVisible = true;
                    if (menuContainer) {
                        menuContainer->SetVisibility(Noesis::Visibility_Visible);
                    }
                    CleanupNPCScripts();
                    aimDotVisible = false;
                    LoadAndPlayMenuMusic();
                    OutputDebugStringA("Returned to menu (Escape pressed)\n");
                }
                escWasPressed = escPressed;

                // === CAMERA (direct control, no actions) ===
                // Camera orbits around player based on mouse input
                XMFLOAT3 charPos = playerChar->GetPositionInterpolated();

                // Calculate camera's right vector (perpendicular to look direction, horizontal)
                float camRightX = cosf(cameraHorizontal); // Right is 90 degrees from forward
                float camRightZ = -sinf(cameraHorizontal);

                // Calculate camera position orbiting around character
                float camOffsetX = sinf(cameraHorizontal) * cosf(cameraVertical) * cameraDistance;
                float camOffsetY = sinf(cameraVertical) * cameraDistance;
                float camOffsetZ = cosf(cameraHorizontal) * cosf(cameraVertical) * cameraDistance;

                // Eye target (what camera looks at) - offset horizontally for over-the-shoulder
                XMFLOAT3 eyeTarget;
                eyeTarget.x = charPos.x + camRightX * cameraHorizontalOffset;
                eyeTarget.y = charPos.y + 1.6f; // Eye level
                eyeTarget.z = charPos.z + camRightZ * cameraHorizontalOffset;

                // Camera position - also offset horizontally
                XMFLOAT3 camPos;
                camPos.x = eyeTarget.x - camOffsetX;
                camPos.y = eyeTarget.y + camOffsetY;
                camPos.z = eyeTarget.z - camOffsetZ;

                // Camera collision check (prevent camera from going through walls)
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

                // Apply camera transform directly (no action system)
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

                // Aim dot fixed at screen center
                aimDotScreenPos.x = GetLogicalWidth() * 0.5f;
                aimDotScreenPos.y = GetLogicalHeight() * 0.5f;
                aimDotVisible = true;

                // Ray from camera in camera's look direction (same as through screen center)
                XMFLOAT3 camLookDir;
                XMStoreFloat3(&camLookDir, camera->GetAt());

                // Raycast from camera in look direction
                wi::primitive::Ray aimRay(camPos, camLookDir, 0.1f, 100.0f);
                auto aimHit = scene.Intersects(
                    aimRay, wi::enums::FILTER_OPAQUE | wi::enums::FILTER_TRANSPARENT, ~0u);

                // Calculate target point (hit or 100 units out)
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

                // Draw debug line (transparent green)
                wi::renderer::RenderableLine debugLine;
                debugLine.start = camPos;
                debugLine.end = targetPoint;
                debugLine.color_start = XMFLOAT4(0, 1, 0, 0.3f); // Green, transparent
                debugLine.color_end = XMFLOAT4(0, 1, 0, 0.3f);
                wi::renderer::DrawLine(debugLine);

                // Check if we hit an NPC
                if (aimHit.entity != wi::ecs::INVALID_ENTITY) {
                    // Check if hit entity or any ancestor is an NPC
                    wi::ecs::Entity checkEntity = aimHit.entity;
                    for (int depth = 0; depth < 10 && checkEntity != wi::ecs::INVALID_ENTITY;
                         ++depth) {
                        // Check if this entity has a MindComponent with NPC type
                        const wi::scene::MindComponent *mind =
                            scene.minds.GetComponent(checkEntity);
                        if (mind != nullptr && mind->IsNPC()) {
                            aimingAtNPC = true;
                            break;
                        }
                        // Check parent via hierarchy
                        const wi::scene::HierarchyComponent *hier =
                            scene.hierarchy.GetComponent(checkEntity);
                        if (hier != nullptr) {
                            checkEntity = hier->parentID;
                        } else {
                            break;
                        }
                    }
                }

                // Update Noesis Talk indicator visibility (only during gameplay)
                if (talkIndicator) {
                    talkIndicator->SetVisibility(
                        (!menuVisible && aimingAtNPC) ? Noesis::Visibility_Visible
                                                      : Noesis::Visibility_Collapsed);
                }
            }

            // Update NPC behavior via Lua scripts (like walkabout)
            if ((patrolScriptLoaded || guardScriptLoaded) && !npcEntities.empty()) {
                for (auto npc : npcEntities) {
                    if (npc == wi::ecs::INVALID_ENTITY) {
                        continue;
                    }
                    // Get the MindComponent to determine which script callback to use
                    const wi::scene::MindComponent *mind = scene.minds.GetComponent(npc);
                    if (mind == nullptr || mind->scriptCallback.empty()) {
                        continue;
                    }
                    // Call the appropriate Lua update function for each NPC
                    // e.g., npc_patrol_update(entity, dt) or npc_guard_update(entity, dt)
                    std::string lua_call = mind->scriptCallback + "(" + std::to_string(npc) + ", " +
                                           std::to_string(dt) + ")";
                    wi::lua::RunText(lua_call);
                }
            }
        }
    }

    void PreRender() override {
        // Call parent PreRender first (sets up Wicked Engine rendering)
        RenderPath3D::PreRender();

        // Noesis offscreen rendering before main render target is bound
        if (uiView && noesisDevice) {
            wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
            wi::graphics::CommandList cmd = device->BeginCommandList();

            // Get D3D12 device and command list
            wi::graphics::GraphicsDevice_DX12 *dx12Device =
                static_cast<wi::graphics::GraphicsDevice_DX12 *>(device);
            ID3D12GraphicsCommandList *d3d12CmdList = dx12Device->GetD3D12CommandList(cmd);

            // Get safe fence value (frame count + buffer count for safety)
            uint64_t safeFenceValue = device->GetFrameCount() + device->GetBufferCount() + 1;

            // Set command list for Noesis (MUST be called before any Noesis rendering)
            NoesisApp::D3D12Factory::SetCommandList(noesisDevice, d3d12CmdList, safeFenceValue);

            // Update Noesis view
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

            // Update render tree and render offscreen textures
            // IMPORTANT: Do this BEFORE Wicked Engine binds its main render target!
            uiView->GetRenderer()->UpdateRenderTree();
            uiView->GetRenderer()->RenderOffscreen();
        }
    }

    void Compose(wi::graphics::CommandList cmd) const override {
        // Call parent Compose first (Wicked Engine composes its layers to backbuffer)
        RenderPath3D::Compose(cmd);

        // Draw aim dot at raycast hit position
        if (!menuVisible && aimDotVisible) {
            wi::image::SetCanvas(*this);

            // Outer circle: 4px radius, semi-transparent
            {
                wi::image::Params params;
                params.pos = XMFLOAT3(aimDotScreenPos.x - 4.0f, aimDotScreenPos.y - 4.0f, 0);
                params.siz = XMFLOAT2(8.0f, 8.0f);
                params.pivot = XMFLOAT2(0, 0);
                params.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f); // White, 30% opacity
                params.blendFlag = wi::enums::BLENDMODE_ALPHA;
                params.enableCornerRounding();
                params.corners_rounding[0] = {4.0f, 8}; // Top-left
                params.corners_rounding[1] = {4.0f, 8}; // Top-right
                params.corners_rounding[2] = {4.0f, 8}; // Bottom-left
                params.corners_rounding[3] = {4.0f, 8}; // Bottom-right
                wi::image::Draw(nullptr, params, cmd);
            }

            // Inner dot: 2px radius, fully opaque
            {
                wi::image::Params params;
                params.pos = XMFLOAT3(aimDotScreenPos.x - 2.0f, aimDotScreenPos.y - 2.0f, 0);
                params.siz = XMFLOAT2(4.0f, 4.0f);
                params.pivot = XMFLOAT2(0, 0);
                params.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // White, fully opaque
                params.blendFlag = wi::enums::BLENDMODE_ALPHA;
                params.enableCornerRounding();
                params.corners_rounding[0] = {2.0f, 8}; // Top-left
                params.corners_rounding[1] = {2.0f, 8}; // Top-right
                params.corners_rounding[2] = {2.0f, 8}; // Bottom-left
                params.corners_rounding[3] = {2.0f, 8}; // Bottom-right
                wi::image::Draw(nullptr, params, cmd);
            }

        }

        // Render Noesis UI on top of everything
        if (uiView && noesisDevice) {
            wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
            wi::graphics::GraphicsDevice_DX12 *dx12Device =
                static_cast<wi::graphics::GraphicsDevice_DX12 *>(device);
            ID3D12GraphicsCommandList *d3d12CmdList = dx12Device->GetD3D12CommandList(cmd);

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
            uiView->SetSize((float)internalResolution.x, (float)internalResolution.y);
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

    void InitializeNoesis() {
        // Initialize Noesis
        Noesis::GUI::SetLicense(NS_LICENSE_NAME, NS_LICENSE_KEY);
        Noesis::GUI::Init();

        // Set texture provider to load images from GUI folder
        Noesis::GUI::SetTextureProvider(Noesis::MakePtr<NoesisApp::LocalTextureProvider>("./GUI"));

        Noesis::GUI::SetXamlProvider(Noesis::MakePtr<NoesisApp::LocalXamlProvider>("./GUI"));

        Noesis::GUI::SetFontProvider(
            Noesis::MakePtr<NoesisApp::LocalFontProvider>("./GUI/Noesis/Data/Theme"));
        const char *fonts[] = {"Fonts/#PT Root UI", "Arial", "Segoe UI Emoji"};
        Noesis::GUI::SetFontFallbacks(fonts, 3);
        Noesis::GUI::SetFontDefaultProperties(14.0f, Noesis::FontWeight_Normal,
                                              Noesis::FontStretch_Normal, Noesis::FontStyle_Normal);

        Noesis::GUI::LoadApplicationResources("Noesis/Data/Theme/NoesisTheme.DarkBlue.xaml");

        Noesis::Uri panelUri("Panel.xaml");
        Noesis::Ptr<Noesis::FrameworkElement> panelRoot =
            Noesis::GUI::LoadXaml<Noesis::FrameworkElement>(panelUri);
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
        menuContainer = FindElementByName<Noesis::Grid>(rootGrid, "MenuContainer");
        seedTextBox = FindElementByName<Noesis::TextBox>(rootGrid, "SeedTextBox");
        playGameButton = FindElementByName<Noesis::Button>(rootGrid, "PlayGameButton");
        fullscreenButton = FindElementByName<Noesis::Button>(rootGrid, "FullscreenButton");
        talkIndicator = FindElementByName<Noesis::FrameworkElement>(rootGrid, "TalkIndicator");

        // Wire up event handlers
        if (playGameButton) {
            playGameButton->Click() +=
                [this](Noesis::BaseComponent *sender, const Noesis::RoutedEventArgs &args) {
                    // Get seed value
                    const char *seedText = seedTextBox ? seedTextBox->GetText() : "12345";

                    char buffer[512];
                    sprintf_s(buffer, "Starting game with seed: %s\n", seedText);
                    OutputDebugStringA(buffer);

                    // Hide the menu
                    menuVisible = false;
                    if (menuContainer) {
                        menuContainer->SetVisibility(Noesis::Visibility_Collapsed);
                    }

                    // Stop menu music
                    if (menuMusicInstance.IsValid()) {
                        wi::audio::Stop(&menuMusicInstance);
                        menuMusicInstance = {};
                        menuMusic = {};
                    }

                    // Load the scene
                    LoadGameScene();

                    // Enable walkabout controls (mouse capture for third-person camera control)
                    SetFirstPersonMode(true);
                };
        }

        // Wire up fullscreen button
        if (fullscreenButton) {
            fullscreenButton->Click() +=
                [this](Noesis::BaseComponent *sender, const Noesis::RoutedEventArgs &args) {
                    ToggleFullscreen();
                };
        }

        // Initial control state update
        UpdateControlStates();

        // Create UserControl to wrap everything
        Noesis::Ptr<Noesis::UserControl> root = Noesis::MakePtr<Noesis::UserControl>();
        root->SetContent(panelRoot);

        // Transparent background
        Noesis::Ptr<Noesis::SolidColorBrush> transparentBrush =
            Noesis::MakePtr<Noesis::SolidColorBrush>();
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
        noesisDevice = NoesisApp::D3D12Factory::CreateDevice(d3d12Device, frameFence, colorFormat,
                                                             stencilFormat, sampleDesc, sRGB);

        // Initialize renderer
        uiView->GetRenderer()->Init(noesisDevice);

        // Get window size from Wicked Engine
        XMUINT2 internalResolution = GetInternalResolution();
        uiView->SetSize((float)internalResolution.x, (float)internalResolution.y);

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
        menuContainer.Reset();
        talkIndicator.Reset();
        rootElement.Reset();

        if (frameFence) {
            frameFence->Release();
            frameFence = nullptr;
        }

        Noesis::GUI::Shutdown();
    }
};
