
function pickHistoricSimDate()
    -- Pick a random day during the quarter
    local quarterMonths = {2, 6, 10}
    simMonth = quarterMonths[simQuarter] + math.random(2)
    simDay = math.random(28)
    -- months and days in Merlin are 0-based, so convert from 1-based Lua values
    mx.setDateAndTime(simYear, simMonth-1, simDay-1, simHour, simMin, 0)
end

function initMerlin(projectPath, commonPath)
    -- Control the console output I want to see
    mx.toggleChannel("mc", "on")
    mx.toggleChannel("belief", "on")
    --mx.toggleChannel("end", "on")
    --mx.toggleChannel("eps", "on")
    mx.toggleChannel("delib", "on")
    --mx.toggleChannel("rule", "on")
    --mx.toggleChannel("rc", "on")
    --mx.toggleChannel("action", "on")
    --mx.toggleChannel("natLang", "on")
    --mx.toggleChannel("boundary", "on")
    --mx.toggleChannel("pattern", "on")
    --mx.toggleChannel("match", "on")
    --mx.toggleChannel("decay", "on")
    --mx.toggleChannel("env", "on")
    --mx.toggleChannel("percept", "on")
    --mx.toggleChannel("parse", "on")
    --mx.toggleChannel("nl", "on")
    --mx.toggleChannel("wme", "on")

    -- Initialize Merlin
    mx.start(projectPath, commonPath)
    mx.toggleChannel("info", "off")
    mx.toggleChannel("warning", "off")

    -- Callbacks
    mxu.safeRegisterCallback("giveBirth", function(arg1, arg2, arg3, arg4) return giveBirth(arg1) end)
    mxu.safeRegisterCallback("siblingRel", function(arg1, arg2, arg3, arg4) return siblingRel(arg1) end)
    mxu.safeRegisterCallback("acquireBuilding", function(arg1, arg2, arg3, arg4) return acquireBuilding(arg1) end)
    mxu.safeRegisterCallback("walkTo", function(arg1, arg2, arg3, arg4) return walkTo(arg1, arg2) end)
    mxu.safeRegisterCallback("turnTo", function(arg1, arg2, arg3, arg4) return turnTo(arg1, arg2) end)

    mxu.safeRegisterSoundCallback(function(arg) return makeSoundBubble(arg) end)

    -- Sync random seed across Lua/Merlin
    mxu.setRandomSeed(42)

    -- NPC traits
    mxu.initMerlinSymbols()
    npcTraitTable = loadAttrTable("NpcTraitTables")
    npcNonTraitTable = loadAttrTable("NpcNonTraitTables")

    -- Start off the sim during the FIRST QUARTER of 1720 (matching NPC birth year)
    firstSimYear = 1720
    simYear = firstSimYear
    simQuarter = 1
    simHour = 6
    simMin = 0.0
    simSec = 0 -- only used by realtime sim
    pickHistoricSimDate()
   
    -- For realtime simulation, use traversal (not teleporting)
    mx.setNpcLocomotionMode("traverse")

    -- Load the game world
    loadScene()
    
    -- Note: NPCs are created separately via createRootNpcs() after player is created
end

function startSim(projectPath, commonPath)
    -- Legacy function - now just calls initMerlin
    initMerlin(projectPath, commonPath)
    createRootNpcs()
end


function endSim()
    -- Dump all NPCs
    --mx.allDumpSelfKnowledge()
    --mx.enterMind(traceNpc)
    --mx.dump()
    mx.stop()
end

function setNpcAlertness(alertness)
    for _, npc in mxu.iter(mx.npcs()) do
        mx.setSymbolAttr(npc, "alertness", alertness)
        --mx.logDebug(mxu.toLuaString(npc) .. " becomes " .. mxu.toLuaString(alertness))
    end
end

function advanceHistoricalSim(luaDt)
    -- Legacy function for historical simulation - kept for backwards compatibility
    -- TODO: 
    --  Some NPCs will leave the island for work or marriage.  
    --    Singles only - no families.
    --  Others will arrive from other places: England & Ireland only, at the moment 
    --    A pair of RootNPCs, to replace when an old person dies.  
    --  This is important since our population is fairly small, to avoid inbreeding
    --
    -- Weekends started becoming a thing in the late 19th century, so at our time in
    -- the game, it would be common that workers worked half-days saturday and then
    -- were off until monday. For now, everyone works between 8AM - 5PM mon - fri.
    -- Later, we can individualize this per job etc.

    -- Since this function calls mx.sim(), which in turn can call Lua functions, we should disable JITTing this function
    jit.off()

    if simYear >= 1897 then
        return false
    end

    if (simYear >= firstSimYear+16) then
        --mx.toggleChannel("delib", "on")
        --mx.toggleChannel("end", "on")
        --mx.toggleChannel("wme", "on")
        --mx.toggleChannel("boundary", "on")
    end
    if (simYear >= firstSimYear+30) then
        --mx.toggleChannel("env", "on")
        --mx.toggleChannel("percept", "on")
    end

    -- Advance the simulation time
    if timeStepMode == realtime then
        simSec = simSec + luaDt
        if simSec >= 60 then
            simSec = simSec - 60
            simMin = simMin + 1
        end
        dt = luaDt
    end
    if timeStepMode == debug then
        simMin = simMin + debugDt / 60.0
        dt = debugDt
    end
    if timeStepMode == historical then
        simMin = simMin + historicalDt / 60.0
        dt = historicalDt
    end

    if simMin >= 60 then
        simMin = simMin - 60
        simHour = simHour + 1
        if simHour == 20 then
            setNpcAlertness(msym.sleepy)
        end
        if simHour >= 24 then
            simHour = 6
            simQuarter = simQuarter + 1
            if simQuarter == 4 then
                simQuarter = 1
                simYear = simYear + 1
                if simYear >= 1897 then
                    return false
                end
            end
            -- Currently we only sim 1 day / quarter in the historic sim
            mx.allSleep();
            pickHistoricSimDate()
            setNpcAlertness(msym.alert)
        end
    end

    mx.setTime(simHour, simMin, simSec)
    mx.sim(1)

    return true
end

function advanceRealtimeSim(luaDt)
    -- Realtime simulation: advance time by actual frame delta and run one sim step per frame
    -- Since this function calls mx.sim(), which in turn can call Lua functions, we should disable JITTing this function
    jit.off()

    -- Advance simulation time by actual frame delta
    simSec = simSec + luaDt
    if simSec >= 60 then
        simSec = simSec - 60
        simMin = simMin + 1
    end
    
    if simMin >= 60 then
        simMin = simMin - 60
        simHour = simHour + 1
        if simHour == 20 then
            setNpcAlertness(msym.sleepy)
        end
        if simHour >= 24 then
            simHour = 0
            simMin = 0
            simSec = 0
        end
    end

    -- Set the current time in Merlin
    mx.setTime(simHour, simMin, simSec)
    
    -- Run one simulation step
    mx.sim(1)
    
    return true
end
