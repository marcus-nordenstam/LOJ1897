#pragma once

#include "GrymEngine.h"
#include <string>
#include <vector>

struct lua_State;

// ---------------------------------------------------------------------------
// Shared C structs for Merlin <-> GRYM entity sync via LuaJIT FFI.
// Layout MUST match the ffi.cdef in main.lua exactly.
// uint64 entity symbols flow through these buffers raw — no double conversion.
// ---------------------------------------------------------------------------
struct EntitySyncEntry {
    uint64_t merlinId;   // Merlin entity symbol (raw uint64, never converted)
    float x, y, z;       // Position in meters
    char modelPath[260]; // Model file path for GRYM rendering (MAX_PATH)
};

struct EntitySyncBuffer {
    static constexpr int CAPACITY = 256;
    EntitySyncEntry entries[CAPACITY];
    int count = 0;
};

// C++ convenience view of one NPC's state (read from npcStateBuffer)
struct NpcState {
    uint64_t merlinId = 0; // Merlin entity symbol (raw uint64)
    float x, y, z;         // Position in meters
    std::string modelPath; // Path to GRYM model file for rendering
};

class MerlinLua {
  public:
    MerlinLua() = default;
    ~MerlinLua() = default;

    // Initialize Merlin Lua subsystem with path to Merlin data directory
    bool Initialize(const std::string &merlin_path);

    // Create player entity in Merlin environment
    void CreatePlayer(const XMFLOAT3 &position);

    // Update player position in Merlin environment each frame
    void UpdatePlayerPosition(const XMFLOAT3 &position);

    // Create NPCs (called after player character is created)
    // spawnPoints: vector of spawn positions (x, y, z in meters)
    // npcModelPath: path to the NPC model file (e.g. .grs) for GRYM rendering
    // waypointPositions: vector of waypoint positions (x, y, z in meters) to inject as knowledge
    void CreateNpcs(const std::vector<XMFLOAT3> &spawnPoints = {},
                    const std::string &npcModelPath = "",
                    const std::vector<XMFLOAT3> &waypointPositions = {});

    // Get all Merlin NPC states (triggers Lua to fill shared buffer, reads it back)
    std::vector<NpcState> GetNpcStates();

    // Position feedback: feed GRYM physics-resolved positions back to Merlin.
    // Call BeginPositionFeedback(), then AddPositionFeedback() for each entity,
    // then ApplyPositionFeedback() to trigger Lua to read the buffer and update Merlin.
    void BeginPositionFeedback();
    void AddPositionFeedback(uint64_t merlinId, const XMFLOAT3 &position);
    void ApplyPositionFeedback();

    // Update Merlin simulation each frame
    void Update(float dt);

    // Shutdown Merlin and close Lua state
    void Shutdown();

    // Check if initialized
    bool IsInitialized() const { return L != nullptr; }

    uint16_t QueryHstr(const char *str);

    // Get Merlin waypoint entity symbols (returns vector of uint64_t symbols, in same order as
    // passed to CreateNpcs)
    std::vector<uint64_t> GetWaypointSymbols();

    // Merlin CInterface function pointers (loaded from Merlin.dll at runtime)
    struct MerlinFloat3 {
        float floats[3];
    };
    typedef MerlinFloat3 (*WorldPosFn)(uint64_t);
    typedef uint64_t (*QueryHstrFn)(const char *);
    typedef void (*RegisterBufferFn)(uint64_t, struct ForeignActionCommandBuffer *);
    typedef void (*UnregisterBufferFn)(uint64_t);

    static WorldPosFn worldPos;
    static QueryHstrFn queryHstr;
    static RegisterBufferFn registerForeignActionCommandBuffer;
    static UnregisterBufferFn unregisterForeignActionCommandBuffer;

  private:
    lua_State *L = nullptr;

    // Shared buffers for Merlin <-> GRYM entity sync.
    // Accessed by Lua via FFI pointers passed during Initialize().
    EntitySyncBuffer npcStateBuffer;    // Lua fills, C++ reads
    EntitySyncBuffer posFeedbackBuffer; // C++ fills, Lua reads
    EntitySyncBuffer waypointBuffer;    // Lua fills, C++ reads (waypoint entity symbols)

    // Load Merlin CInterface functions from DLL
    bool LoadMerlinCInterface();
};
