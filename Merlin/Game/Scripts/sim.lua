
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
