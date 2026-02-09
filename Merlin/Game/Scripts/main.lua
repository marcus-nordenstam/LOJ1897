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

function merlinCreateNpcs(spawnPoints, npcModelPath)
    -- Create NPCs at spawn points from scene metadata
    -- spawnPoints: table of {x, y, z} positions in meters
    -- npcModelPath: path to NPC model file (e.g. .grs) for GRYM rendering
    if spawnPoints == nil then
        spawnPoints = {}
    end
    if npcModelPath == nil then
        npcModelPath = ""
    end
    createRootNpcs(spawnPoints, npcModelPath)
end

function merlinGetNpcStates()
    -- Return a table of all Merlin NPC states for GRYM proximity spawning
    local states = {}
    for _, npc in mxu.iter(mx.npcs()) do
        local pos = mx.worldPos(npc)
        local npcId = tostring(npc)  -- unique Merlin symbol as string key
        
        -- Get modelPath attribute (default to empty string if not set)
        local modelPathAttr = mx.attrSymbol(npc, "modelPath")
        local modelPath = ""
        if modelPathAttr ~= nil and modelPathAttr ~= 0 then
            modelPath = mxu.toLuaString(modelPathAttr)
        end
        
        table.insert(states, {
            id = npcId,
            x = pos.floats[0],
            y = pos.floats[1],
            z = pos.floats[2],
            modelPath = modelPath
        })
    end
    return states
end

function merlinUpdate(dt)
    -- Run one timestep of simulation per frame
    advanceRealtimeSim(dt)
end

function merlinShutdown()
    endSim()
end
