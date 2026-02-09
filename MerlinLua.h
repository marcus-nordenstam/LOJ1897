#pragma once

#include <string>

struct lua_State;

class MerlinLua {
public:
    MerlinLua() = default;
    ~MerlinLua() = default;

    // Initialize Merlin Lua subsystem with path to Merlin data directory
    bool Initialize(const std::string& merlin_path);
    
    // Create NPCs (called after player character is created)
    void CreateNpcs();
    
    // Update Merlin simulation each frame
    void Update(float dt);
    
    // Shutdown Merlin and close Lua state
    void Shutdown();
    
    // Check if initialized
    bool IsInitialized() const { return L != nullptr; }

private:
    lua_State* L = nullptr;
};
