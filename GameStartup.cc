#include "GameStartup.h"

#include "common/grProjectInit.h"
#include <Systems/animation_system.h>
#include <Systems/character_system.h>
#include <wiPhysics.h>

#include <NsGui/Enums.h>

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <unordered_set>

// Include Merlin's shared header for action buffer communication (pure C, no Merlin internals)
#include "../Merlin/Src/Lib/Interface/C/ForeignActionCommandBuffer.h"

// Forward declaration for action dispatch handler
static void HandleWalkTo(wi::scene::Scene &scene, wi::scene::CharacterComponent &character,
                         ForeignActionCommand &cmd,
                         std::unordered_map<uint64_t, GrymMerlinEntityEntry> &entityTable);

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

    // Shutdown Merlin Lua subsystem
    merlinLua.Shutdown();

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
        configFile << "# LOJ1897 Configuration File\n";
        configFile << "# Library paths are stored in wiProject.ini in the project directory\n\n";

        // LOJ1897 section with project path
        configFile << "[LOJ1897]\n";
        configFile << "project_path = " << GetProjectPath() << "\n\n";

        // LOJ-game specific settings
        configFile << "[game]\n";
        configFile << "theme_music = " << themeMusic << "\n";
        configFile << "level = " << levelPath << "\n";
        configFile << "player_model = " << playerModel << "\n";
        configFile << "npc_model = " << npcModel << "\n";
        configFile.close();

        char buffer[512];
        sprintf_s(buffer, "Saved config: project_path = %s\n", GetProjectPath().c_str());
        wi::backlog::post(buffer);
    }
}

void GameStartup::LoadConfig() {
    std::string configPath = GetConfigFilePath();

    // Use wi::config::File to load config.ini
    wi::config::File config;
    if (!config.Open(configPath.c_str())) {
        wi::backlog::post("ERROR: Failed to open config.ini\n");
        UpdateControlStates();
        return;
    }

    // Initialize the project using the split config pattern
    std::string projectPath;
    if (!grym::InitializeProjectFromSplitConfig(config, "LOJ1897", projectPath)) {
        wi::backlog::post("ERROR: Failed to initialize project from split config\n");
        UpdateControlStates();
        return;
    }

    // Now read LOJ-game specific settings from the config
    if (config.HasSection("game")) {
        auto &game = config.GetSection("game");
        themeMusic = game.GetText("theme_music");
        levelPath = game.GetText("level");
        playerModel = game.GetText("player_model");
        npcModel = game.GetText("npc_model");
    }

    char buffer[512];
    sprintf_s(buffer,
              "Loaded LOJ-game config:\n  project_path = %s\n  theme_music = %s\n  level = %s\n",
              GetProjectPath().c_str(), themeMusic.c_str(), levelPath.c_str());
    wi::backlog::post(buffer);

    UpdateControlStates();
}

void GameStartup::InitializeAnimationSystem(wi::scene::Scene &scene) {
    // Load animation library
    const std::string animLib = wi::Project::ptr()->animation_library_full_path();
    if (!animLib.empty()) {
        char buffer[512];
        sprintf_s(buffer, "Loading animation library: %s\n", animLib.c_str());
        wi::backlog::post(buffer);

        if (wi::helper::FileExists(animLib)) {
            // Animation library is now loaded automatically by Project
            // Just trigger the load to make sure it's up to date
            int count = wi::Project::ptr()->LoadAnimationLibrary();

            sprintf_s(buffer, "Animation library loaded with %d animations\n", count);
            wi::backlog::post(buffer);

            // List all loaded animations
            for (size_t i = 0; i < wi::Project::ptr()->GetAnimationCount(); ++i) {
                const wi::scene::Animation *anim =
                    wi::Project::ptr()->GetAnimation(static_cast<wi::scene::AnimationIndex>(i));
                if (anim != nullptr) {
                    sprintf_s(buffer, "Found animation '%s'\n", anim->name.c_str());
                    wi::backlog::post(buffer);
                }
            }
        } else {
            sprintf_s(buffer, "ERROR: Animation library not found: %s\n", animLib.c_str());
            wi::backlog::post(buffer);
        }
    } else {
        wi::backlog::post("No animation library (anim_lib) configured in config.ini\n");
    }

    // Load expressions
    const std::string expressionPath = wi::Project::ptr()->expression_library_full_path();
    if (!expressionPath.empty()) {
        char buffer[512];
        sprintf_s(buffer, "Loading expressions from: %s\n", expressionPath.c_str());
        wi::backlog::post(buffer);
        scene.LoadExpressions();
        wi::backlog::post("Expressions loaded successfully\n");
    } else {
        wi::backlog::post("No expression path (expression_path) configured in config.ini\n");
    }
}

void GameStartup::UpdateControlStates() {
    bool hasProjectPath = !GetProjectPath().empty();

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

    std::string musicPath = GetProjectPath();
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
        }
        // NPC spawn points are collected and used to create Merlin NPCs in LoadGameScene
        // Visual GRYM entities are spawned dynamically via UpdateProximitySpawning
    }
}

wi::ecs::Entity GameStartup::SpawnCharacter(wi::scene::Scene &scene, const std::string &modelPath,
                                            const XMFLOAT3 &position, const XMFLOAT3 &forward,
                                            bool isPlayer) {
    char buffer[512];
    
    // Use prefab instantiation instead of LoadModel
    sprintf_s(buffer, "Instantiating character prefab: %s\n", modelPath.c_str());
    wi::backlog::post(buffer);

    std::string projectPath = GetProjectPath();
    wi::ecs::Entity modelRoot = scene.InstantiateNew(modelPath, projectPath);

    if (modelRoot == wi::ecs::INVALID_ENTITY) {
        sprintf_s(buffer, "ERROR: Failed to instantiate character prefab: %s\n", modelPath.c_str());
        wi::backlog::post(buffer);
        return wi::ecs::INVALID_ENTITY;
    }

    // Find humanoid component
    wi::ecs::Entity humanoidEntity = wi::ecs::INVALID_ENTITY;
    for (size_t h = 0; h < scene.cc_humanoids.GetCount(); ++h) {
        wi::ecs::Entity hEntity = scene.cc_humanoids.GetEntity(h);
        if (hEntity == modelRoot || scene.Entity_IsDescendant(hEntity, modelRoot)) {
            humanoidEntity = hEntity;
            break;
        }
    }

    scene.ResetPose(modelRoot);

    // Ensure lookAt is disabled for all game characters (player and NPCs)
    // regardless of what the loaded model has
    if (humanoidEntity != wi::ecs::INVALID_ENTITY) {
        wi::scene::HumanoidComponent *humanoid = scene.cc_humanoids.GetComponent(humanoidEntity);
        if (humanoid != nullptr) {
            humanoid->SetLookAtEnabled(false);
        }
    }

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
    }
    transform->ClearTransform();
    transform->Translate(position);
    transform->UpdateTransform();

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

        // Script callbacks disabled — NPCs are now driven by Merlin action pipeline
        // mind.scriptCallback is left empty so actions control movement directly

        npcEntities.push_back(characterEntity);
    }

    // Store animation library indices for character
    if (modelRoot != wi::ecs::INVALID_ENTITY && humanoidEntity != wi::ecs::INVALID_ENTITY) {
        // Store humanoid entity reference for animation playback component
        character.humanoidEntity = humanoidEntity;

        // Determine character gender from model path (simplified: check for "fem")
        std::string modelPathLower = modelPath;
        std::transform(modelPathLower.begin(), modelPathLower.end(), modelPathLower.begin(),
                       ::tolower);
        bool isFemale = (modelPathLower.find("fem") != std::string::npos);

        auto *project = wi::Project::ptr();
        if (project != nullptr) {
            // Find gender-appropriate animations
            if (isFemale) {
                character.action_anim_indices[(int)wi::scene::ActionVerb::Idle] =
                    project->FindAnimationBySubstrings({"_fem", "idle", "01"});
                character.action_anim_indices[(int)wi::scene::ActionVerb::Walk] =
                    project->FindAnimationBySubstrings({"_fem", "walk", "normal"});
                character.action_anim_indices[(int)wi::scene::ActionVerb::Run] =
                    project->FindAnimationBySubstrings({"_fem", "run"});
            } else {
                character.action_anim_indices[(int)wi::scene::ActionVerb::Idle] =
                    project->FindAnimationBySubstrings({"_male", "idle", "02"});
                character.action_anim_indices[(int)wi::scene::ActionVerb::Walk] =
                    project->FindAnimationBySubstrings({"_male", "walk", "normal"});
                character.action_anim_indices[(int)wi::scene::ActionVerb::Run] =
                    project->FindAnimationBySubstrings({"_male", "run"});
            }

            // Gender-neutral animations
            wi::scene::AnimationIndex sit_anim = project->FindAnimationBySubstrings({"sit"});
            character.action_anim_indices[(int)wi::scene::ActionVerb::Sit] = sit_anim;
            character.action_anim_indices[(int)wi::scene::ActionVerb::Stand] = sit_anim;

            // WalkTo uses same animation as Walk
            character.action_anim_indices[(int)wi::scene::ActionVerb::WalkTo] =
                character.action_anim_indices[(int)wi::scene::ActionVerb::Walk];
        }

        // Set initial action (animations will be retargeted on-demand by character system)
        character.SetAction(scene, wi::scene::character_system::make_idle(scene, character));
    }

    // Bind expressions
    if (modelRoot != wi::ecs::INVALID_ENTITY && !scene.unbound_expressions.empty()) {
        int mapped_count = 0;
        for (size_t expr_index = 0; expr_index < scene.unbound_expressions.size(); expr_index++) {
            const auto &expr = scene.unbound_expressions[expr_index];
            auto morph_entities = scene.find_entities_with_morphs(modelRoot, expr);
            for (auto target_entity : morph_entities) {
                auto expr_comp = scene.demand_expression_component(target_entity);
                if (expr_comp->bind_expression(scene, expr_index, target_entity)) {
                    mapped_count++;
                }
            }
        }
    }

    sprintf_s(buffer, "%s character spawned successfully (Entity: %llu)\n",
              isPlayer ? "Player" : "NPC", characterEntity);
    wi::backlog::post(buffer);

    return characterEntity;
}

void GameStartup::LoadNPCScripts() {}

void GameStartup::CleanupNPCScripts() { npcEntities.clear(); }

void GameStartup::LoadGameScene(wi::scene::Scene &scene) {
    auto startTime = std::chrono::high_resolution_clock::now();

    // Set the master scene in the project so materials can be properly managed
    wi::Project::ptr()->master_scene = &scene;

    if (levelPath.empty()) {
        wi::backlog::post("ERROR: No level path configured in config.ini\n");
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        char buffer[512];
        sprintf_s(buffer, "LoadGameScene aborted (no level path) in %lld ms\n", duration.count());
        wi::backlog::post(buffer);
        return;
    }

    std::string scenePath = GetProjectPath();
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

        // DEBUG: Dump weather entities, their children (sun/moon lights), and all directional
        // lights
        {
            sprintf_s(buffer, "[Sky DEBUG] Weather count: %zu\n", scene.weathers.GetCount());
            wi::backlog::post(buffer);
            for (size_t i = 0; i < scene.weathers.GetCount(); ++i) {
                wi::ecs::Entity weatherEntity = scene.weathers.GetEntity(i);
                wi::scene::NameComponent *weatherName = scene.names.GetComponent(weatherEntity);
                sprintf_s(buffer, "[Sky DEBUG] Weather[%zu] entity=%u name='%s'\n", i,
                          (unsigned)weatherEntity,
                          weatherName ? weatherName->name.c_str() : "<unnamed>");
                wi::backlog::post(buffer);

                // List children of this weather entity
                wi::vector<wi::ecs::Entity> children;
                scene.GatherChildren(weatherEntity, children);
                sprintf_s(buffer, "[Sky DEBUG]   Children count: %zu\n", children.size());
                wi::backlog::post(buffer);
                for (size_t c = 0; c < children.size(); ++c) {
                    wi::ecs::Entity child = children[c];
                    wi::scene::NameComponent *childName = scene.names.GetComponent(child);
                    wi::scene::LightComponent *childLight = scene.lights.GetComponent(child);
                    sprintf_s(buffer,
                              "[Sky DEBUG]   Child[%zu] entity=%u name='%s' hasLight=%s type=%s "
                              "intensity=%.2f\n",
                              c, (unsigned)child, childName ? childName->name.c_str() : "<unnamed>",
                              childLight ? "YES" : "no",
                              childLight
                                  ? (childLight->type == wi::scene::LightComponent::DIRECTIONAL
                                         ? "DIRECTIONAL"
                                     : childLight->type == wi::scene::LightComponent::POINT
                                         ? "POINT"
                                         : "SPOT")
                                  : "n/a",
                              childLight ? childLight->intensity : 0.0f);
                    wi::backlog::post(buffer);
                }
            }

            // Also dump ALL directional lights in the scene
            sprintf_s(buffer, "[Sky DEBUG] Total lights in scene: %zu\n", scene.lights.GetCount());
            wi::backlog::post(buffer);
            size_t dirCount = 0;
            for (size_t i = 0; i < scene.lights.GetCount(); ++i) {
                const wi::scene::LightComponent &light = scene.lights[i];
                if (light.type == wi::scene::LightComponent::DIRECTIONAL) {
                    wi::ecs::Entity lightEntity = scene.lights.GetEntity(i);
                    wi::scene::NameComponent *lightName = scene.names.GetComponent(lightEntity);
                    wi::scene::HierarchyComponent *hier = scene.hierarchy.GetComponent(lightEntity);
                    wi::ecs::Entity parentEntity = hier ? hier->parentID : wi::ecs::INVALID_ENTITY;
                    wi::scene::NameComponent *parentName =
                        parentEntity != wi::ecs::INVALID_ENTITY
                            ? scene.names.GetComponent(parentEntity)
                            : nullptr;
                    sprintf_s(buffer,
                              "[Sky DEBUG] DirectionalLight[%zu] entity=%u name='%s' "
                              "intensity=%.2f color=(%.2f,%.2f,%.2f) parent=%u parentName='%s'\n",
                              dirCount, (unsigned)lightEntity,
                              lightName ? lightName->name.c_str() : "<unnamed>", light.intensity,
                              light.color.x, light.color.y, light.color.z, (unsigned)parentEntity,
                              parentName ? parentName->name.c_str() : "<unnamed>");
                    wi::backlog::post(buffer);
                    dirCount++;
                }
            }
            sprintf_s(buffer, "[Sky DEBUG] Total directional lights: %zu\n", dirCount);
            wi::backlog::post(buffer);
        }

        // Wait for resource loading jobs to complete
        wi::jobsystem::Wait(wi::jobsystem::context());

        InitializeAnimationSystem(scene);

        // Wait for any animation/expression loading jobs
        wi::jobsystem::Wait(wi::jobsystem::context());

        // After scene deserialization, resources are loaded with IMPORT_DELAY flag,
        // meaning they have handles but no actual GPU data yet.
        // We must call CreateRenderData() on all materials to trigger actual texture loading
        // before Scene::Update() tries to access them via RunMaterialUpdateSystem.
        auto project = wi::Project::ptr();
        for (size_t i = 0; i < project->material_library.size(); ++i) {
            project->material_library[i].CreateRenderData();
        }
        wi::jobsystem::Wait(wi::jobsystem::context());
        sprintf_s(buffer, "Material render data created for %zu materials\n",
                  project->material_library.size());
        wi::backlog::post(buffer);

        // CRITICAL: Find spawn position and set camera BEFORE first update
        // This ensures terrain generates chunks at the correct location
        XMFLOAT3 spawnPosition = XMFLOAT3(0, 0, 0);
        for (size_t i = 0; i < scene.metadatas.GetCount(); i++) {
            const wi::scene::MetadataComponent &metadata = scene.metadatas[i];
            if (metadata.preset == wi::scene::MetadataComponent::Preset::Player) {
                wi::scene::TransformComponent *spawnTransform =
                    scene.transforms.GetComponent(scene.metadatas.GetEntity(i));
                if (spawnTransform) {
                    spawnPosition = spawnTransform->GetPosition();
                    break;
                }
            }
        }

        // Position camera at spawn location so terrain generates correct chunks
        scene.camera.Eye = spawnPosition;
        scene.camera.At = XMFLOAT3(spawnPosition.x, spawnPosition.y, spawnPosition.z + 1.0f);
        scene.camera.UpdateCamera();

        sprintf_s(buffer, "Camera positioned at spawn: (%.2f, %.2f, %.2f)\n", spawnPosition.x,
                  spawnPosition.y, spawnPosition.z);
        wi::backlog::post(buffer);

        // First update with dt=0 to initialize non-terrain systems
        scene.Update(0.0f);

        // DEBUG: Check what the weather system resolved after first Update
        {
            sprintf_s(buffer,
                      "[Sky DEBUG] After Update(0): weather.most_important_light_index=%u "
                      "moon_light_index=%u\n",
                      scene.weather.most_important_light_index, scene.weather.moon_light_index);
            wi::backlog::post(buffer);
            sprintf_s(buffer, "[Sky DEBUG] sunColor=(%.3f,%.3f,%.3f) sunDir=(%.3f,%.3f,%.3f)\n",
                      scene.weather.sunColor.x, scene.weather.sunColor.y, scene.weather.sunColor.z,
                      scene.weather.sunDirection.x, scene.weather.sunDirection.y,
                      scene.weather.sunDirection.z);
            wi::backlog::post(buffer);
            sprintf_s(buffer, "[Sky DEBUG] moonColor=(%.3f,%.3f,%.3f) moonDir=(%.3f,%.3f,%.3f)\n",
                      scene.weather.moonColor.x, scene.weather.moonColor.y,
                      scene.weather.moonColor.z, scene.weather.moonDirection.x,
                      scene.weather.moonDirection.y, scene.weather.moonDirection.z);
            wi::backlog::post(buffer);
        }

        // CRITICAL: Update with small positive dt to trigger terrain generation!
        // Terrain Generation_Update only runs when dt > 0
        scene.Update(0.016f); // ~60fps timestep

        // Wait for terrain generation jobs to complete
        wi::jobsystem::Wait(wi::jobsystem::context());

        // Additional updates to ensure terrain BVH is built and ready
        for (int i = 0; i < 3; i++) {
            scene.Update(0.016f);
            wi::jobsystem::Wait(wi::jobsystem::context());
        }

        // Enable physics simulation for character collision with ground
        wi::physics::SetSimulationEnabled(true);

        // Initialize Merlin Lua subsystem (before creating characters)
        std::string exePath = wi::helper::GetExecutablePath();
        std::string exeDir = wi::helper::GetDirectoryFromPath(exePath);
        std::string merlin_path = exeDir + "Merlin";
        merlinLua.Initialize(merlin_path);

        // Spawn player character from metadata
        SpawnCharactersFromMetadata(scene);

        // Create player entity in Merlin environment
        if (merlinLua.IsInitialized() && playerCharacter != wi::ecs::INVALID_ENTITY) {
            wi::scene::TransformComponent *playerTransform =
                scene.transforms.GetComponent(playerCharacter);
            if (playerTransform) {
                XMFLOAT3 playerPos = playerTransform->GetPosition();
                merlinLua.CreatePlayer(playerPos);
            }
        }

        // Collect NPC spawn points from metadata for Merlin NPCs
        struct NPCSpawnInfo {
            XMFLOAT3 position;
            XMFLOAT3 forward;
        };
        std::vector<NPCSpawnInfo> npcSpawnInfos;
        std::vector<XMFLOAT3> npcSpawnPoints;
        std::vector<XMFLOAT3> waypointPositions;
        std::vector<wi::ecs::Entity> waypointEntities;

        for (size_t i = 0; i < scene.metadatas.GetCount(); i++) {
            wi::ecs::Entity entity = scene.metadatas.GetEntity(i);
            const wi::scene::MetadataComponent &metadata = scene.metadatas[i];

            if (metadata.preset == wi::scene::MetadataComponent::Preset::NPC) {
                wi::scene::TransformComponent *spawnTransform =
                    scene.transforms.GetComponent(entity);
                if (spawnTransform) {
                    NPCSpawnInfo spawnInfo;
                    spawnInfo.position = spawnTransform->GetPosition();
                    spawnInfo.forward = spawnTransform->GetForward();
                    npcSpawnInfos.push_back(spawnInfo);
                    npcSpawnPoints.push_back(spawnTransform->GetPosition());
                }
            } else if (metadata.preset == wi::scene::MetadataComponent::Preset::Waypoint) {
                wi::scene::TransformComponent *waypointTransform =
                    scene.transforms.GetComponent(entity);
                if (waypointTransform) {
                    waypointPositions.push_back(waypointTransform->GetPosition());
                    waypointEntities.push_back(entity);
                }
            }
        }

        // Create Merlin NPCs after player character is created
        // Visual GRYM entities will be spawned/despawned dynamically via UpdateProximitySpawning
        if (merlinLua.IsInitialized() && playerCharacter != wi::ecs::INVALID_ENTITY) {
            // Parse npcModel as space-separated list of model paths
            std::vector<std::string> npcModelPaths;
            std::istringstream iss(npcModel);
            std::string modelPath;
            while (iss >> modelPath) {
                npcModelPaths.push_back(modelPath);
            }
            
            merlinLua.CreateNpcs(npcSpawnPoints, npcModelPaths, waypointPositions);

            // Register waypoint GRYM entities in grym_merlin_entity_table
            // Lua has filled the waypoint buffer with Merlin waypoint entity symbols (in same order
            // as waypointPositions)
            std::vector<uint64_t> waypointSymbols = merlinLua.GetWaypointSymbols();
            if (waypointSymbols.size() != waypointEntities.size()) {
                wi::backlog::post("[warning] Waypoint count mismatch between GRYM and Merlin\n");
            }
            for (size_t i = 0; i < waypointSymbols.size() && i < waypointEntities.size(); i++) {
                GrymMerlinEntityEntry entry;
                entry.grymEntity = waypointEntities[i];
                entry.outOfRangeTimer = 0.0f;
                entry.actionBuffer = nullptr; // Waypoints don't have action buffers
                grym_merlin_entity_table[waypointSymbols[i]] = entry;
            }

            // Initialize action dispatch table
            actionDispatchTable[merlinLua.QueryHstr("WALK_TO")] = &HandleWalkTo;
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

void GameStartup::FeedbackGrymPositions(wi::scene::Scene &scene) {
    // Feed GRYM physics-resolved positions back to Merlin for all spawned entities.
    // This ensures gravity, collisions, etc. resolved by GRYM are reflected in Merlin.
    merlinLua.BeginPositionFeedback();

    for (const auto &[merlinId, entry] : grym_merlin_entity_table) {
        if (entry.grymEntity == wi::ecs::INVALID_ENTITY)
            continue;

        wi::scene::CharacterComponent *character = scene.characters.GetComponent(entry.grymEntity);
        if (character) {
            XMFLOAT3 pos = character->GetPositionInterpolated();
            merlinLua.AddPositionFeedback(merlinId, pos);
        }
    }

    merlinLua.ApplyPositionFeedback();
}

void GameStartup::UpdateProximitySpawning(wi::scene::Scene &scene, const XMFLOAT3 &playerPos,
                                          float dt) {
    // Query all Merlin NPC states
    std::vector<NpcState> npcStates = merlinLua.GetNpcStates();

    // Track which Merlin NPCs we've seen this frame
    std::unordered_set<uint64_t> activeNpcs;

    const float spawnRange = 100.0f;  // meters
    const float despawnDelay = 10.0f; // seconds

    for (const auto &npcState : npcStates) {
        activeNpcs.insert(npcState.merlinId);

        // Compute distance to player (use Merlin position for proximity check)
        XMFLOAT3 npcPos(npcState.x, npcState.y, npcState.z);
        XMVECTOR playerVec = XMLoadFloat3(&playerPos);
        XMVECTOR npcVec = XMLoadFloat3(&npcPos);
        XMVECTOR distVec = XMVector3Length(npcVec - playerVec);
        float distance = XMVectorGetX(distVec);

        // Check if already in table
        auto it = grym_merlin_entity_table.find(npcState.merlinId);
        bool isSpawned = (it != grym_merlin_entity_table.end());

        if (distance <= spawnRange) {
            // Within spawn range
            if (!isSpawned) {
                // Spawn new GRYM entity at Merlin's position (initial placement only)
                if (!npcState.modelPath.empty()) {
                    XMFLOAT3 forward(0, 0, 1); // Default forward direction
                    wi::ecs::Entity grymEntity =
                        SpawnCharacter(scene, npcState.modelPath, npcPos, forward, false);

                    if (grymEntity != wi::ecs::INVALID_ENTITY) {
                        GrymMerlinEntityEntry entry;
                        entry.grymEntity = grymEntity;
                        entry.outOfRangeTimer = 0.0f;

                        // Allocate action command buffer for this NPC
                        entry.actionBuffer = new ForeignActionCommandBuffer{};
                        memset(entry.actionBuffer, 0, sizeof(ForeignActionCommandBuffer));

                        grym_merlin_entity_table[npcState.merlinId] = entry;

                        // Register buffer with Merlin so action rules can write to it
                        MerlinLua::registerForeignActionCommandBuffer(npcState.merlinId,
                                                                      entry.actionBuffer);

                        // Add to npcEntities tracking vector
                        npcEntities.push_back(grymEntity);

                        char buffer[1024];
                        sprintf_s(buffer,
                                  "Spawned GRYM entity %llu for Merlin NPC %llu at (%.2f, %.2f, %.2f) distance %.2fm\n",
                                  static_cast<unsigned long long>(grymEntity),
                                  static_cast<unsigned long long>(npcState.merlinId),
                                  npcPos.x, npcPos.y, npcPos.z, distance);
                        wi::backlog::post(buffer);
                        
                        // Check if entity has visual components and layer mask
                        int objectCount = 0;
                        for (size_t i = 0; i < scene.objects.GetCount(); i++) {
                            wi::ecs::Entity ent = scene.objects.GetEntity(i);
                            if (ent == grymEntity || scene.Entity_IsDescendant(ent, grymEntity)) {
                                objectCount++;
                            }
                        }
                        
                        auto* layer = scene.layers.GetComponent(grymEntity);
                        auto* transform = scene.transforms.GetComponent(grymEntity);
                        
                        sprintf_s(buffer, "  Entity %llu: objects=%d layerMask=0x%X\n",
                                  static_cast<unsigned long long>(grymEntity), objectCount,
                                  layer ? layer->layerMask : 0);
                        wi::backlog::post(buffer);
                        
                        if (transform) {
                            XMFLOAT3 pos = transform->GetPosition();
                            sprintf_s(buffer, "  Transform position: (%.2f, %.2f, %.2f)\n",
                                      pos.x, pos.y, pos.z);
                            wi::backlog::post(buffer);
                        }
                    }
                }
            } else {
                // Already spawned — do NOT overwrite GRYM position.
                // GRYM owns the real-time physics (gravity, collisions).
                // Positions are fed back to Merlin via FeedbackGrymPositions().
                it->second.outOfRangeTimer = 0.0f;
            }
        } else {
            // Outside spawn range
            if (isSpawned) {
                // Increment out-of-range timer
                it->second.outOfRangeTimer += dt;

                // Despawn if out of range for too long
                if (it->second.outOfRangeTimer >= despawnDelay) {
                    wi::ecs::Entity grymEntity = it->second.grymEntity;

                    // Unregister and delete action buffer
                    if (it->second.actionBuffer) {
                        MerlinLua::unregisterForeignActionCommandBuffer(npcState.merlinId);
                        delete it->second.actionBuffer;
                    }

                    // Remove from scene
                    scene.Entity_Remove(grymEntity, true);

                    // Remove from npcEntities vector
                    auto npcIt = std::find(npcEntities.begin(), npcEntities.end(), grymEntity);
                    if (npcIt != npcEntities.end()) {
                        npcEntities.erase(npcIt);
                    }

                    // Remove from table
                    grym_merlin_entity_table.erase(it);

                    char buffer[256];
                    sprintf_s(buffer, "Despawned GRYM entity %llu for Merlin NPC %llu (out of range for %.1fs)\n",
                              static_cast<unsigned long long>(grymEntity),
                              static_cast<unsigned long long>(npcState.merlinId),
                              it->second.outOfRangeTimer);
                    wi::backlog::post(buffer);
                }
            }
            // If not spawned and out of range: no-op
        }
    }

    // Clean up stale entries for Merlin NPCs that no longer exist
    std::vector<uint64_t> toRemove;
    for (const auto &entry : grym_merlin_entity_table) {
        if (activeNpcs.find(entry.first) == activeNpcs.end()) {
            toRemove.push_back(entry.first);
        }
    }

    for (uint64_t merlinId : toRemove) {
        auto it = grym_merlin_entity_table.find(merlinId);
        if (it != grym_merlin_entity_table.end()) {
            wi::ecs::Entity grymEntity = it->second.grymEntity;

            // Unregister and delete action buffer
            if (it->second.actionBuffer) {
                MerlinLua::unregisterForeignActionCommandBuffer(merlinId);
                delete it->second.actionBuffer;
            }

            scene.Entity_Remove(grymEntity, true);

            auto npcIt = std::find(npcEntities.begin(), npcEntities.end(), grymEntity);
            if (npcIt != npcEntities.end()) {
                npcEntities.erase(npcIt);
            }

            grym_merlin_entity_table.erase(it);

            char buffer[256];
            sprintf_s(buffer, "Removed stale GRYM entity %llu for Merlin NPC %llu (NPC no longer exists)\n",
                      static_cast<unsigned long long>(grymEntity),
                      static_cast<unsigned long long>(merlinId));
            wi::backlog::post(buffer);
        }
    }
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

// ============================================================================
// Action Dispatch Handlers
// ============================================================================

static void HandleWalkTo(wi::scene::Scene &scene, wi::scene::CharacterComponent &character,
                         ForeignActionCommand &cmd,
                         std::unordered_map<uint64_t, GrymMerlinEntityEntry> &entityTable) {
    // Resolve target position — depends on Merlin symbol type
    XMFLOAT3 targetPos;
    bool useEntityWalkTo = false;
    wi::ecs::Entity grymTarget = wi::ecs::INVALID_ENTITY;

    // First, try entity table lookup (works for entities like waypoints, NPCs, etc.)
    auto it = entityTable.find(cmd.target);
    if (it != entityTable.end()) {
        grymTarget = it->second.grymEntity;
        auto *transform = scene.transforms.GetComponent(grymTarget);
        if (transform) {
            targetPos = transform->GetPosition();
            useEntityWalkTo = true;
        }
    }

    if (!useEntityWalkTo) {
        // Target is likely an OBB (spatial bounds) or other non-entity symbol.
        // Use Merlin's worldPos() CInterface to extract the position.
        auto mpos = MerlinLua::worldPos(cmd.target);
        targetPos = XMFLOAT3(mpos.floats[0], mpos.floats[1], mpos.floats[2]);
    }

    // DEBUG: Check which path we're taking and animation setup
    static bool debuggedOnce = false;
    if (!debuggedOnce) {
        debuggedOnce = true;
        char buffer[512];
        sprintf_s(
            buffer,
            "[HandleWalkTo] useEntityWalkTo=%d, grymTarget=%llu, targetPos=(%.2f,%.2f,%.2f)\n",
            useEntityWalkTo, grymTarget, targetPos.x, targetPos.y, targetPos.z);
        wi::backlog::post(buffer);
        sprintf_s(buffer, "[HandleWalkTo] NPC humanoid=%llu, Walk anim=%d, WalkTo anim=%d\n",
                  character.humanoidEntity,
                  character.action_anim_indices[(int)wi::scene::ActionVerb::Walk],
                  character.action_anim_indices[(int)wi::scene::ActionVerb::WalkTo]);
        wi::backlog::post(buffer);
    }

    // Check arrival (same 1.5m threshold GRYM uses in update_walkto_action)
    XMFLOAT3 currentPos = character.GetPositionInterpolated();
    float dx = targetPos.x - currentPos.x;
    float dz = targetPos.z - currentPos.z;
    float distSq = dx * dx + dz * dz;

    if (distSq <= 1.5f * 1.5f) {
        // Arrived — signal success and go idle
        cmd.outcome = ACTION_OUTCOME_SUCCESS;
        character.SetAction(scene, wi::scene::character_system::make_idle(scene, character));
        return;
    }

    // Set the GRYM action (similar pattern to player input in NoesisRenderPath)
    if (useEntityWalkTo) {
        // Entity target: use make_walk_to which reads the entity's transform each frame
        if (!character.IsWalking()) {
            auto action = wi::scene::character_system::make_walk_to(scene, character, grymTarget);
            character.SetAction(scene, action);
        } else {
            // Already walking - make_walk_to updates target automatically, no action needed
        }
    } else {
        // OBB target: compute direction and use make_walk
        float dist = sqrtf(distSq);
        XMFLOAT3 dir = {dx / dist, 0.0f, dz / dist};

        if (!character.IsWalking()) {
            auto action = wi::scene::character_system::make_walk(scene, character, dir);
            character.SetAction(scene, action);
        } else {
            // Already walking - just update direction
            character.curActions[(int)wi::scene::ActionMotor::Body].direction = dir;
        }
    }
}

void GameStartup::ProcessActionCommands(wi::scene::Scene &scene) {
    for (auto &[merlinId, entry] : grym_merlin_entity_table) {
        if (!entry.actionBuffer)
            continue;
        auto *character = scene.characters.GetComponent(entry.grymEntity);
        if (!character)
            continue;

        for (int motor = 0; motor < ForeignActionCommandBuffer::NUM_MOTORS; motor++) {
            auto &cmd = entry.actionBuffer->commands[motor];
            if (!cmd.active)
                continue;

            auto it = actionDispatchTable.find(cmd.actionLabelHstr);
            if (it != actionDispatchTable.end()) {
                it->second(scene, *character, cmd, grym_merlin_entity_table);
            }
        }
    }
}
