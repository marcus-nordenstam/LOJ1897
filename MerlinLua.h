#pragma once

#include <string>
#include <vector>
#include "GrymEngine.h"

struct lua_State;

// Merlin NPC state for C++ querying
struct NpcState {
    std::string merlinId;  // Unique Merlin entity ID (string representation of symbol)
    float x, y, z;         // Position in meters
    std::string modelPath; // Path to GRYM model file for rendering
};

class MerlinLua {
public:
    MerlinLua() = default;
    ~MerlinLua() = default;

    // Initialize Merlin Lua subsystem with path to Merlin data directory
    bool Initialize(const std::string& merlin_path);
    
    // Create player entity in Merlin environment
    void CreatePlayer(const XMFLOAT3& position);
    
    // Update player position in Merlin environment each frame
    void UpdatePlayerPosition(const XMFLOAT3& position);
    
    // Create NPCs (called after player character is created)
    // spawnPoints: vector of spawn positions (x, y, z in meters)
    // npcModelPath: path to the NPC model file (e.g. .grs) for GRYM rendering
    void CreateNpcs(const std::vector<XMFLOAT3>& spawnPoints = {}, const std::string& npcModelPath = "");
    
    // Get all Merlin NPC states (for proximity spawning)
    std::vector<NpcState> GetNpcStates();
    
    // Update Merlin simulation each frame
    void Update(float dt);
    
    // Shutdown Merlin and close Lua state
    void Shutdown();
    
    // Check if initialized
    bool IsInitialized() const { return L != nullptr; }

private:
    lua_State* L = nullptr;
};
