#pragma once

#include "WickedEngine.h"

#include <NsGui/Button.h>
#include <NsGui/Grid.h>
#include <NsGui/TextBox.h>

#include <functional>
#include <string>
#include <vector>

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

    // Helper to find animation entity by name substring
    wi::ecs::Entity FindAnimationByName(const wi::scene::Scene &scene,
                                        const char *anim_name_substr);

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

    // Animation library entities
    std::vector<wi::ecs::Entity> animationLibrary;

    // Music playback
    wi::audio::Sound menuMusic;
    wi::audio::SoundInstance menuMusicInstance;

    // Player character tracking
    wi::ecs::Entity playerCharacter = wi::ecs::INVALID_ENTITY;

    // NPC tracking
    std::vector<wi::ecs::Entity> npcEntities;
    bool patrolScriptLoaded = false;
    bool guardScriptLoaded = false;

    // Fullscreen state
    bool isFullscreen = false;
    RECT windowedRect = {};
    DWORD windowedStyle = 0;
    DWORD windowedExStyle = 0;

    // Callbacks
    PlayGameCallback playGameCallback;
    FullscreenCallback fullscreenCallback;
};
