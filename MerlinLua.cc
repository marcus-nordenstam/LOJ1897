#include "MerlinLua.h"

#include "GrymEngine.h"

#include <Windows.h>
#include <filesystem>
#include <lua.hpp>

// Initialize static function pointers
MerlinLua::WorldPosFn MerlinLua::worldPos = nullptr;
MerlinLua::QueryHstrFn MerlinLua::queryHstr = nullptr;
MerlinLua::RegisterBufferFn MerlinLua::registerForeignActionCommandBuffer = nullptr;
MerlinLua::UnregisterBufferFn MerlinLua::unregisterForeignActionCommandBuffer = nullptr;

// Pure C print function - no C++ objects on the stack
static int merlin_lua_print(lua_State *L) {
    char buf[2048];
    int pos = 0;
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        if (i > 1 && pos < 2046)
            buf[pos++] = '\t';
        const char *s = lua_tostring(L, i);
        if (s) {
            while (*s && pos < 2046)
                buf[pos++] = *s++;
        }
    }
    if (pos < 2047)
        buf[pos++] = '\n';
    buf[pos] = '\0';
    wi::backlog::post(buf);
    return 0;
}

bool MerlinLua::Initialize(const std::string &merlin_path) {
    if (L != nullptr) {
        wi::backlog::post("MerlinLua already initialized\n");
        return false;
    }

    // Create new Lua state with all standard libraries
    L = luaL_newstate();
    if (!L) {
        wi::backlog::post("ERROR: Failed to create Lua state for Merlin\n");
        return false;
    }

    luaL_openlibs(L);

    // Redirect Lua print() to wi::backlog::post() using a pure C function
    lua_pushcfunction(L, merlin_lua_print);
    lua_setglobal(L, "print");

    // Get the current executable directory for cwd (where Merlin.dll lives)
    std::string exe_path = wi::helper::GetExecutablePath();
    std::string exe_dir = wi::helper::GetDirectoryFromPath(exe_path);

    // Set global 'cwd' to exe directory so mx.lua can find Merlin.dll
    lua_pushstring(L, exe_dir.c_str());
    lua_setglobal(L, "cwd");

    // Set global 'merlin_path' so Lua scripts can find Merlin data tree
    lua_pushstring(L, merlin_path.c_str());
    lua_setglobal(L, "merlin_path");

    // Pass shared buffer pointers to Lua as lightuserdata.
    // Lua will cast these to FFI struct pointers for direct read/write.
    // uint64 entity symbols flow through these buffers raw — no double conversion.
    lua_pushlightuserdata(L, &npcStateBuffer);
    lua_setglobal(L, "g_npcStateBufferPtr");
    lua_pushlightuserdata(L, &posFeedbackBuffer);
    lua_setglobal(L, "g_posFeedbackBufferPtr");
    lua_pushlightuserdata(L, &waypointBuffer);
    lua_setglobal(L, "g_waypointBufferPtr");

    // Adjust package.path to include Merlin scripts and Lua directory
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    std::string current_path = lua_tostring(L, -1);
    lua_pop(L, 1);

    // Add Merlin/Game/Scripts/?.lua and Lua/?.lua to package.path
    std::string new_path =
        current_path + ";" + merlin_path + "/Game/Scripts/?.lua" + ";" + exe_dir + "/Lua/?.lua";

    lua_pushstring(L, new_path.c_str());
    lua_setfield(L, -2, "path");
    lua_pop(L, 1); // pop package table

    // Load and run main.lua which sets up the Merlin environment
    std::string main_lua_path = merlin_path + "/Game/Scripts/main.lua";

    if (luaL_dofile(L, main_lua_path.c_str()) != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        char buffer[512];
        sprintf_s(buffer, "ERROR: Failed to load Merlin main.lua: %s\n",
                  error_msg ? error_msg : "unknown error");
        wi::backlog::post(buffer);

        // Try to get stack trace
        lua_getglobal(L, "debug");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "traceback");
            if (lua_isfunction(L, -1)) {
                lua_pushvalue(L, -3);
                lua_call(L, 1, 1);
                const char *traceback = lua_tostring(L, -1);
                sprintf_s(buffer, "Stack trace:\n%s\n", traceback ? traceback : "<no traceback>");
                wi::backlog::post(buffer);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        lua_pop(L, 1);
        lua_close(L);
        L = nullptr;
        return false;
    }

    // Call merlinInit() to start the simulation
    lua_getglobal(L, "merlinInit");
    if (!lua_isfunction(L, -1)) {
        char buffer[512];
        int type = lua_type(L, -1);
        sprintf_s(buffer, "ERROR: merlinInit is not a function (type=%s)\n", lua_typename(L, type));
        wi::backlog::post(buffer);
        lua_pop(L, 1);
        lua_close(L);
        L = nullptr;
        return false;
    }

    // Disable JIT compilation (Merlin FFI callbacks re-enter Lua, which can cause issues with JIT)
    luaL_dostring(L, "jit.off()");

    // Load Merlin CInterface functions from DLL
    if (!LoadMerlinCInterface()) {
        wi::backlog::post("ERROR: Failed to load Merlin CInterface functions\n");
        lua_close(L);
        L = nullptr;
        return false;
    }

    // Call merlinInit()
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        char buffer[512];
        sprintf_s(buffer, "ERROR: merlinInit() failed: %s\n",
                  error_msg ? error_msg : "unknown error");
        wi::backlog::post(buffer);
        lua_pop(L, 1);
        lua_close(L);
        L = nullptr;
        return false;
    }

    return true;
}

void MerlinLua::CreatePlayer(const XMFLOAT3 &position) {
    if (!L) {
        wi::backlog::post("ERROR: Cannot create player - Merlin Lua not initialized\n");
        return;
    }

    // Call merlinCreatePlayer(x, y, z) to create player entity in Merlin
    lua_getglobal(L, "merlinCreatePlayer");
    if (!lua_isfunction(L, -1)) {
        char buffer[512];
        int type = lua_type(L, -1);
        sprintf_s(buffer, "ERROR: merlinCreatePlayer is not a function (type=%s)\n",
                  lua_typename(L, type));
        wi::backlog::post(buffer);
        lua_pop(L, 1);
        return;
    }

    // Push position arguments (in meters)
    lua_pushnumber(L, position.x);
    lua_pushnumber(L, position.y);
    lua_pushnumber(L, position.z);

    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        char buffer[512];
        sprintf_s(buffer, "ERROR: merlinCreatePlayer() failed: %s\n",
                  error_msg ? error_msg : "unknown error");
        wi::backlog::post(buffer);
        lua_pop(L, 1);
    } else {
        char buffer[256];
        sprintf_s(buffer, "Merlin player entity created at (%.2f, %.2f, %.2f)\n", position.x,
                  position.y, position.z);
        wi::backlog::post(buffer);
    }
}

void MerlinLua::UpdatePlayerPosition(const XMFLOAT3 &position) {
    if (!L) {
        return;
    }

    // Call merlinUpdatePlayerPos(x, y, z) to update player position in Merlin
    lua_getglobal(L, "merlinUpdatePlayerPos");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    // Push position arguments (in meters)
    lua_pushnumber(L, position.x);
    lua_pushnumber(L, position.y);
    lua_pushnumber(L, position.z);

    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        char buffer[512];
        sprintf_s(buffer, "ERROR: merlinUpdatePlayerPos() failed: %s\n",
                  error_msg ? error_msg : "unknown error");
        wi::backlog::post(buffer);
        lua_pop(L, 1);
    }
}

void MerlinLua::CreateNpcs(const std::vector<XMFLOAT3> &spawnPoints,
                           const std::vector<std::string> &npcModelPaths,
                           const std::vector<XMFLOAT3> &waypointPositions) {
    if (!L) {
        wi::backlog::post("ERROR: Cannot create NPCs - Merlin Lua not initialized\n");
        return;
    }

    // Call merlinCreateNpcs(spawnPoints, npcModelPaths, waypoints)
    lua_getglobal(L, "merlinCreateNpcs");
    if (!lua_isfunction(L, -1)) {
        char buffer[512];
        int type = lua_type(L, -1);
        sprintf_s(buffer, "ERROR: merlinCreateNpcs is not a function (type=%s)\n",
                  lua_typename(L, type));
        wi::backlog::post(buffer);
        lua_pop(L, 1);
        return;
    }

    // Push spawn points table to Lua (positions in meters)
    lua_newtable(L);
    for (size_t i = 0; i < spawnPoints.size(); i++) {
        lua_pushinteger(L, i + 1); // Lua uses 1-based indexing
        lua_newtable(L);

        // Push x, y, z (in meters)
        lua_pushstring(L, "x");
        lua_pushnumber(L, spawnPoints[i].x);
        lua_settable(L, -3);

        lua_pushstring(L, "y");
        lua_pushnumber(L, spawnPoints[i].y);
        lua_settable(L, -3);

        lua_pushstring(L, "z");
        lua_pushnumber(L, spawnPoints[i].z);
        lua_settable(L, -3);

        lua_settable(L, -3);
    }

    // Push npcModelPaths table to Lua
    lua_newtable(L);
    for (size_t i = 0; i < npcModelPaths.size(); i++) {
        lua_pushinteger(L, i + 1); // Lua uses 1-based indexing
        lua_pushstring(L, npcModelPaths[i].c_str());
        lua_settable(L, -3);
    }

    // Push waypoints table to Lua (positions in meters)
    lua_newtable(L);
    for (size_t i = 0; i < waypointPositions.size(); i++) {
        lua_pushinteger(L, i + 1); // Lua uses 1-based indexing
        lua_newtable(L);

        // Push x, y, z (in meters)
        lua_pushstring(L, "x");
        lua_pushnumber(L, waypointPositions[i].x);
        lua_settable(L, -3);

        lua_pushstring(L, "y");
        lua_pushnumber(L, waypointPositions[i].y);
        lua_settable(L, -3);

        lua_pushstring(L, "z");
        lua_pushnumber(L, waypointPositions[i].z);
        lua_settable(L, -3);

        lua_settable(L, -3);
    }

    char buffer[512];
    sprintf_s(buffer, "Creating Merlin NPCs at %zu spawn points with %zu model(s) (%zu waypoints)\n",
              spawnPoints.size(), npcModelPaths.size(), waypointPositions.size());
    wi::backlog::post(buffer);

    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        sprintf_s(buffer, "ERROR: merlinCreateNpcs() failed: %s\n",
                  error_msg ? error_msg : "unknown error");
        wi::backlog::post(buffer);
        lua_pop(L, 1);
    } else {
        sprintf_s(buffer,
                  "Merlin NPCs created successfully at %zu spawn points with %zu waypoints\n",
                  spawnPoints.size(), waypointPositions.size());
        wi::backlog::post(buffer);
    }
}

std::vector<NpcState> MerlinLua::GetNpcStates() {
    std::vector<NpcState> states;

    if (!L) {
        return states;
    }

    // Trigger Lua to fill the shared npcStateBuffer (no data through the Lua stack)
    lua_getglobal(L, "merlinGetNpcStates");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return states;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        char buffer[512];
        sprintf_s(buffer, "ERROR: merlinGetNpcStates() failed: %s\n",
                  error_msg ? error_msg : "unknown error");
        wi::backlog::post(buffer);
        lua_pop(L, 1);
        return states;
    }

    // Read from the shared buffer — uint64 IDs come through with exact bits intact
    states.reserve(npcStateBuffer.count);
    for (int i = 0; i < npcStateBuffer.count; i++) {
        NpcState state;
        state.merlinId = npcStateBuffer.entries[i].merlinId;
        state.x = npcStateBuffer.entries[i].x;
        state.y = npcStateBuffer.entries[i].y;
        state.z = npcStateBuffer.entries[i].z;
        state.modelPath = npcStateBuffer.entries[i].modelPath;
        states.push_back(state);
    }

    return states;
}

void MerlinLua::BeginPositionFeedback() { posFeedbackBuffer.count = 0; }

void MerlinLua::AddPositionFeedback(uint64_t merlinId, const XMFLOAT3 &position) {
    if (posFeedbackBuffer.count >= EntitySyncBuffer::CAPACITY)
        return;

    int idx = posFeedbackBuffer.count++;
    posFeedbackBuffer.entries[idx].merlinId = merlinId;
    posFeedbackBuffer.entries[idx].x = position.x;
    posFeedbackBuffer.entries[idx].y = position.y;
    posFeedbackBuffer.entries[idx].z = position.z;
}

void MerlinLua::ApplyPositionFeedback() {
    if (!L || posFeedbackBuffer.count == 0)
        return;

    // Trigger Lua to read the posFeedbackBuffer and apply positions to Merlin entities
    lua_getglobal(L, "merlinApplyPosFeedback");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        char buffer[512];
        sprintf_s(buffer, "ERROR: merlinApplyPosFeedback() failed: %s\n",
                  error_msg ? error_msg : "unknown error");
        wi::backlog::post(buffer);
        lua_pop(L, 1);
    }
}

void MerlinLua::Update(float dt) {
    if (!L) {
        return;
    }

    // Call merlinUpdate(dt) each frame
    lua_getglobal(L, "merlinUpdate");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_pushnumber(L, dt);

    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        char buffer[512];
        sprintf_s(buffer, "ERROR: merlinUpdate() failed: %s\n",
                  error_msg ? error_msg : "unknown error");
        wi::backlog::post(buffer);
        lua_pop(L, 1);
    }
}

void MerlinLua::Shutdown() {
    if (!L) {
        return;
    }

    // Call merlinShutdown() to cleanly stop the simulation
    lua_getglobal(L, "merlinShutdown");
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char *error_msg = lua_tostring(L, -1);
            char buffer[512];
            sprintf_s(buffer, "WARNING: merlinShutdown() failed: %s\n",
                      error_msg ? error_msg : "unknown error");
            wi::backlog::post(buffer);
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }

    // Close Lua state
    lua_close(L);
    L = nullptr;

    wi::backlog::post("Merlin Lua subsystem shut down\n");
}

uint16_t MerlinLua::QueryHstr(const char *str) {
    if (!queryHstr) {
        wi::backlog::post("ERROR: queryHstr function not loaded from Merlin.dll\n");
        return 0;
    }
    return queryHstr(str);
}

std::vector<uint64_t> MerlinLua::GetWaypointSymbols() {
    std::vector<uint64_t> symbols;
    if (!L)
        return symbols;

    // waypointBuffer is already filled by createRootNpcs (via FFI)
    symbols.reserve(waypointBuffer.count);
    for (int i = 0; i < waypointBuffer.count; i++) {
        symbols.push_back(waypointBuffer.entries[i].merlinId);
    }

    return symbols;
}

bool MerlinLua::LoadMerlinCInterface() {
    // Load Merlin.dll (should already be loaded by Lua, but get handle)
    HMODULE merlinDll = GetModuleHandleA("Merlin.dll");
    if (!merlinDll) {
        wi::backlog::post("ERROR: Merlin.dll not loaded\n");
        return false;
    }

    // Load CInterface functions
    worldPos = (WorldPosFn)GetProcAddress(merlinDll, "worldPos");
    queryHstr = (QueryHstrFn)GetProcAddress(merlinDll, "queryHstr");
    registerForeignActionCommandBuffer =
        (RegisterBufferFn)GetProcAddress(merlinDll, "registerForeignActionCommandBuffer");
    unregisterForeignActionCommandBuffer =
        (UnregisterBufferFn)GetProcAddress(merlinDll, "unregisterForeignActionCommandBuffer");

    if (!worldPos || !queryHstr || !registerForeignActionCommandBuffer ||
        !unregisterForeignActionCommandBuffer) {
        char buffer[512];
        sprintf_s(buffer,
                  "ERROR: Failed to load Merlin CInterface functions: worldPos=%p queryHstr=%p "
                  "register=%p unregister=%p\n",
                  worldPos, queryHstr, registerForeignActionCommandBuffer,
                  unregisterForeignActionCommandBuffer);
        wi::backlog::post(buffer);
        return false;
    }

    wi::backlog::post("Merlin CInterface functions loaded successfully\n");
    return true;
}
