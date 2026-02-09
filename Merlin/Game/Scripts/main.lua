-- Paths set by C++ before this file runs:
--   cwd          = directory containing Merlin.dll (exe dir)
--   merlin_path  = path to the Merlin/ data directory

-- projectPath is where the project-specific Behaviour files are (Merlin/Game)
-- commonPath is where the common Behaviour files are (Merlin/Common)
-- Make these global so merlinInit() can access them
projectPath = merlin_path .. "/Game"
commonPath  = merlin_path .. "/Common"

-- Load Merlin FFI bridge and utilities (scripts are in Game/Scripts/)
dofile(merlin_path .. "/Game/Scripts/mx.lua")
dofile(merlin_path .. "/Game/Scripts/mxu.lua")
dofile(merlin_path .. "/Game/Scripts/npc.lua")
dofile(merlin_path .. "/Game/Scripts/doc.lua")
dofile(merlin_path .. "/Game/Scripts/scene.lua")
dofile(merlin_path .. "/Game/Scripts/sim.lua")

require("globals")
-- NO drawing, rendering, or GUI modules loaded

-- ---------------------------------------------------------------------------
-- Shared C buffers for Merlin <-> GRYM entity sync.
-- Layout matches EntitySyncEntry / EntitySyncBuffer in MerlinLua.h exactly.
-- uint64 entity symbols stay as raw uint64 — no double/string conversion.
-- ---------------------------------------------------------------------------
ffi.cdef[[
    typedef struct {
        uint64_t merlinId;
        float x, y, z;
        char modelPath[260];
    } EntitySyncEntry;

    typedef struct {
        EntitySyncEntry entries[256];
        int count;
    } EntitySyncBuffer;
]]

-- Cast lightuserdata pointers (set by C++ in MerlinLua::Initialize) to FFI struct pointers
local npcStateBuf    = ffi.cast("EntitySyncBuffer*", g_npcStateBufferPtr)
local posFeedbackBuf = ffi.cast("EntitySyncBuffer*", g_posFeedbackBufferPtr)
waypointBuf          = ffi.cast("EntitySyncBuffer*", g_waypointBufferPtr)

-- Global player entity
playerEntity = nil

function merlinInit()
    -- Initialize Merlin but don't create NPCs yet
    initMerlin(projectPath, commonPath)
end

function merlinCreatePlayer(x, y, z)
    -- Create player entity in Merlin environment at given position (in meters)
    local playerPos = mxu.float3(x, y, z)
    local playerScale = mxu.float3(0.3, 1.0, 1.75)  -- width, height, depth in meters
    local playerQuat = mxu.quat(0, 0, 0, 1)  -- identity quaternion
    local playerObb = mx.composeBounds(playerPos, playerScale, playerQuat)
    
    -- Create the player entity
    local birthTime = mx.makeSeconds(1720, 3, 0, 6, 0, 0)  -- Same time as NPCs
    playerEntity = mx.makeRootEntityAtTime("human_player", "human", birthTime, playerObb)
    mx.setOccluder(playerEntity, false)
    
    -- Set basic attributes
    mx.setSymbolAttr(playerEntity, "name", mx.hstrSymbol("Player"))
    mx.setSymbolAttr(playerEntity, "gender", mx.hstrSymbol("male"))
    
    print("Player entity created in Merlin environment")
end

function merlinUpdatePlayerPos(x, y, z)
    -- Update player position in Merlin environment (called each frame)
    if playerEntity == nil then
        return
    end
    
    -- Get current OBB and update position
    local playerObb = mx.worldBounds(playerEntity)
    local playerSize = mxu.float3(playerObb.floats[3], playerObb.floats[4], playerObb.floats[5])
    local playerQuat = mxu.quat(playerObb.floats[6], playerObb.floats[7], playerObb.floats[8], playerObb.floats[9])
    local newPos = mxu.float3(x, y, z)
    local newObb = mx.composeBounds(newPos, playerSize, playerQuat)
    
    mx.setLocalBoundsAttr(playerEntity, "obb", newObb)
end

function merlinCreateNpcs(spawnPoints, npcModelPath, waypointPositions)
    -- Create NPCs at spawn points from scene metadata
    -- spawnPoints: table of {x, y, z} positions in meters
    -- npcModelPath: path to NPC model file (e.g. .grs) for GRYM rendering
    -- waypointPositions: table of {x, y, z} waypoint positions in meters
    if spawnPoints == nil then
        spawnPoints = {}
    end
    if npcModelPath == nil then
        npcModelPath = ""
    end
    if waypointPositions == nil then
        waypointPositions = {}
    end
    createRootNpcs(spawnPoints, npcModelPath, waypointPositions)
end

function merlinGetNpcStates()
    -- Fill the shared npcStateBuffer with current NPC states.
    -- uint64 merlinId stays as raw uint64 in the buffer — no conversion.
    local idx = 0
    for _, npc in mxu.iter(mx.npcs()) do
        if idx >= 256 then break end
        
        local pos = mx.worldPos(npc)
        npcStateBuf.entries[idx].merlinId = npc  -- raw uint64 symbol, no conversion
        npcStateBuf.entries[idx].x = pos.floats[0]
        npcStateBuf.entries[idx].y = pos.floats[1]
        npcStateBuf.entries[idx].z = pos.floats[2]
        
        -- Get modelPath attribute
        local modelPathAttr = mx.attrSymbol(npc, "modelPath")
        if modelPathAttr ~= nil and modelPathAttr ~= 0 then
            local mp = mxu.toLuaString(modelPathAttr)
            if #mp < 260 then
                ffi.copy(npcStateBuf.entries[idx].modelPath, mp)
            else
                ffi.copy(npcStateBuf.entries[idx].modelPath, mp, 259)
                npcStateBuf.entries[idx].modelPath[259] = 0
            end
        else
            npcStateBuf.entries[idx].modelPath[0] = 0
        end
        
        idx = idx + 1
    end
    npcStateBuf.count = idx
end

function merlinApplyPosFeedback()
    -- Read GRYM physics-resolved positions from the shared posFeedbackBuffer
    -- and apply them to the corresponding Merlin entities.
    -- uint64 merlinId is read as raw uint64 cdata — directly usable as Merlin symbol.
    for i = 0, posFeedbackBuf.count - 1 do
        local entry = posFeedbackBuf.entries[i]
        local entity = entry.merlinId  -- uint64 cdata, directly usable with mx.* FFI calls
        
        local obb = mx.worldBounds(entity)
        local size = mxu.float3(obb.floats[3], obb.floats[4], obb.floats[5])
        local quat = mxu.quat(obb.floats[6], obb.floats[7], obb.floats[8], obb.floats[9])
        local newPos = mxu.float3(entry.x, entry.y, entry.z)
        local newObb = mx.composeBounds(newPos, size, quat)
        
        mx.setLocalBoundsAttr(entity, "obb", newObb)
    end
end

function merlinUpdate(dt)
    -- Run one timestep of simulation per frame
    advanceRealtimeSim(dt)
end

function merlinShutdown()
    endSim()
end
