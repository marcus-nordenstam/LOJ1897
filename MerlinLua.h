#pragma once

#include <string>
#include <vector>
#include "GrymEngine.h"

struct lua_State;

class MerlinLua {
public:
    MerlinLua() = default;
    ~MerlinLua() = default;

    // Initialize Merlin Lua subsystem with path to Merlin data directory
    bool Initialize(const std::string& merlin_path);
    
    // Create NPCs (called after player character is created)
    // spawnPoints: vector of spawn positions (x, y, z in meters, will be converted to cm)
    void CreateNpcs(const std::vector<XMFLOAT3>& spawnPoints = {});
    
    // Update Merlin simulation each frame
    void Update(float dt);
    
    // Shutdown Merlin and close Lua state
    void Shutdown();
    
    // Check if initialized
    bool IsInitialized() const { return L != nullptr; }

private:
    lua_State* L = nullptr;
};
