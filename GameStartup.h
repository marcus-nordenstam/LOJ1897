#pragma once

#include "GrymEngine.h"
#include "MerlinLua.h"

#include <NsGui/Button.h>
#include <NsGui/Grid.h>
#include <NsGui/TextBox.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
struct ForeignActionCommandBuffer;
struct ForeignActionCommand;

// Generic entry for mapping Merlin environment entities to GRYM scene entities
struct GrymMerlinEntityEntry {
    wi::ecs::Entity grymEntity = wi::ecs::INVALID_ENTITY;
    float outOfRangeTimer = 0.0f; // seconds this entity has been outside spawn range
    ForeignActionCommandBuffer *actionBuffer = nullptr; // Owned by this entry (for NPCs only)
};

// Game startup/shutdown system for config, scene loading, characters, NPC scripts, music
class GameStartup {
  public:
    GameStartup() = default;
    ~GameStartup() = default;

    // Initialize with UI elements from XAML
    void Initialize(Noesis::Grid *menuContainer, Noesis::TextBox *seedTextBox,
                    Noesis::Button *playButton, Noesis::Button *fullscreenButton);

    // Shutdown and release resources
    void Shutdown();

    // Config file management
    std::string GetConfigFilePath();
    void SaveConfig();
    void LoadConfig();

    // Animation system initialization
    void InitializeAnimationSystem(wi::scene::Scene &scene);

    // Update control states based on project path
    void UpdateControlStates();

    // Music playback
    void LoadAndPlayMenuMusic();
    void StopMenuMusic();
    bool IsMusicPlaying() const { return menuMusicInstance.IsValid(); }

    // Spawn characters from scene metadata
    void SpawnCharactersFromMetadata(wi::scene::Scene &scene);

    // Spawn a single character
    wi::ecs::Entity SpawnCharacter(wi::scene::Scene &scene, const std::string &modelPath,
                                   const XMFLOAT3 &position, const XMFLOAT3 &forward,
                                   bool isPlayer);

    // NPC script management
    void LoadNPCScripts();
    void CleanupNPCScripts();

    // Load the game scene
    void LoadGameScene(wi::scene::Scene &scene);

    // Toggle fullscreen mode
    void ToggleFullscreen(HWND windowHandle);

    // Proximity-based spawning/despawning of GRYM entities from Merlin environment
    void UpdateProximitySpawning(wi::scene::Scene &scene, const XMFLOAT3 &playerPos, float dt);

    // Feed GRYM physics-resolved positions back to Merlin for all entities in
    // grym_merlin_entity_table
    void FeedbackGrymPositions(wi::scene::Scene &scene);

    // Process Merlin action commands and dispatch to GRYM character system
    void ProcessActionCommands(wi::scene::Scene &scene);

    // Getters for state
    const std::string &GetProjectPath() const { return wi::Project::ptr()->path; }
    const std::string &GetLevelPath() const { return levelPath; }
    wi::ecs::Entity GetPlayerCharacter() const { return playerCharacter; }
    const std::vector<wi::ecs::Entity> &GetNPCEntities() const { return npcEntities; }
    bool IsPatrolScriptLoaded() const { return patrolScriptLoaded; }
    bool IsGuardScriptLoaded() const { return guardScriptLoaded; }

    // Callback for play button
    using PlayGameCallback = std::function<void(const std::string &seed)>;
    void SetPlayGameCallback(PlayGameCallback callback) { playGameCallback = callback; }

    // Callback for fullscreen button
    using FullscreenCallback = std::function<void()>;
    void SetFullscreenCallback(FullscreenCallback callback) { fullscreenCallback = callback; }

    // Set fullscreen state (for button text update)
    void SetFullscreenState(bool isFullscreen);

    // Show/hide menu container
    void ShowMenu(bool visible);

    // Merlin Lua subsystem
    MerlinLua merlinLua;

  private:
    // UI elements
    Noesis::Ptr<Noesis::Grid> menuContainer;
    Noesis::Ptr<Noesis::TextBox> seedTextBox;
    Noesis::Ptr<Noesis::Button> playGameButton;
    Noesis::Ptr<Noesis::Button> fullscreenButton;

    // Saved settings
    std::string themeMusic;
    std::string levelPath;
    std::string playerModel;
    std::string npcModel;

    // Music playback
    wi::audio::Sound menuMusic;
    wi::audio::SoundInstance menuMusicInstance;

    // Player character tracking
    wi::ecs::Entity playerCharacter = wi::ecs::INVALID_ENTITY;

    // NPC tracking
    std::vector<wi::ecs::Entity> npcEntities;
    bool patrolScriptLoaded = false;
    bool guardScriptLoaded = false;

    // Generic bidirectional mapping between Merlin environment entities and GRYM scene entities
    // Key: Merlin entity symbol (raw uint64, exact bits — no lossy conversion)
    // Value: GRYM entity and despawn timer
    std::unordered_map<uint64_t, GrymMerlinEntityEntry> grym_merlin_entity_table;

    // Action dispatch: maps Merlin action label symbol → GRYM handler function
    using ActionHandler = void (*)(wi::scene::Scene &, wi::scene::CharacterComponent &,
                                   ForeignActionCommand &,
                                   std::unordered_map<uint64_t, GrymMerlinEntityEntry> &);
    std::unordered_map<uint64_t, ActionHandler> actionDispatchTable;

    // Fullscreen state
    bool isFullscreen = false;
    RECT windowedRect = {};
    DWORD windowedStyle = 0;
    DWORD windowedExStyle = 0;

    // Callbacks
    PlayGameCallback playGameCallback;
    FullscreenCallback fullscreenCallback;
};
