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

function merlinInit()
    -- Initialize Merlin but don't create NPCs yet
    initMerlin(projectPath, commonPath)
end

function merlinCreateNpcs()
    -- Create the 4 root NPCs (called after player character is created)
    createRootNpcs()
end

function merlinUpdate(dt)
    -- Run one timestep of simulation per frame
    advanceRealtimeSim(dt)
end

function merlinShutdown()
    endSim()
end
