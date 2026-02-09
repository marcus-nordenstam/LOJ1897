
local function vecLength(x, y) return math.sqrt(x*x + y*y) end
local function vecNormalize(x, y)
    local l = vecLength(x, y)
    if l > 0 then return x/l, y/l else return 0, 0 end
end
local function vecScale(x, y, s) return x*s, y*s end
local function vecAdd(x1,y1,x2,y2) return x1+x2, y1+y2 end
local function vecSub(x1,y1,x2,y2) return x1-x2, y1-y2 end

function printManors()
    mx.logDebug("Dumping all manors")
    for index, ent in mxu.iter(manors) do
        mx.logDebug(mxu.toLuaString(ent))
    end
end

function loadScene()
    -- Load the scene layout (hand-placed spaces, buildings, props etc)
    kindToSystemTable = {
        ["space"]="space",
        ["structure"]="struct",
        ["collection stack"]="stack",
        ["structurePart"]="part",
        ["*"]="prop"
    }

    -- Create a world directly from the terrain file
    -- 100m wide sectors (in cm)
    mx.makeWorldFromTerrain("Env/island.ter", 10000)
    -- Load the scene layout (objects, buildings, spaces, set dressing etc)
    mxu.loadSceneLayout("Env/layout.dat", kindToSystemTable)

    -- Gather things that we use in the setup
    spaces = mx.findEntitiesOfKind("space", "space")
    manors = mx.findEntitiesOfKind("struct", "manor")
    cottages = mx.findEntitiesOfKind("struct", "cottage")
    terraces = mx.findEntitiesOfKind("struct", "terrace")
    theatres = mx.findEntitiesOfKind("struct", "building theatre")

    allBuildings = {}
    for index, ent in mxu.iter(manors) do
        table.insert(allBuildings, ent)
    end
    for index, ent in mxu.iter(cottages) do
        table.insert(allBuildings, ent)
    end
    for index, ent in mxu.iter(terraces) do
        table.insert(allBuildings, ent)
    end
    for index, ent in mxu.iter(theatres) do
        table.insert(allBuildings, ent)
    end

    sectors = mx.worldSectors()
end

function acquireBuilding(bldgKind)
    for index, ent in mxu.iter(manors) do
        if (mx.isASymbol(ent, bldgKind)) then
            mx.removeSymbolByIndex(manors, index-1) -- convert to C index
            return ent
        end
    end
    for index, ent in mxu.iter(cottages) do
        if (mx.isASymbol(ent, bldgKind)) then
            mx.removeSymbolByIndex(cottages, index-1) -- convert to C index
            return ent
        end
    end
    for index, ent in mxu.iter(terraces) do
        if (mx.isASymbol(ent, bldgKind)) then
            mx.removeSymbolByIndex(terraces, index-1) -- convert to C index
            return ent
        end
    end
    mx.logDebug("Building acquisition FAILED!")
    debug.debug()
    return 0
end

function makeSoundBubble(sound)
    local obb = mx.worldBounds(sound)
    local speaker = mx.attrSymbol(sound, "speaker")
    table.insert(soundBubbles, {
        pos = mxu.obbPos(obb),
        rad = mxu.obbRadius(obb),
        msg = ffi.string(mx.nlMsg(sound) or ""),
        t = os.clock(),
        age = 0,
        life = 5,
        speaker = speaker
    })
end

-- Push overlapping NPCs apart, keeping fixedNpc stationary
function resolveNpcOverlaps(fixedNpc, iterations, tolerance)
    iterations = iterations or 10
    tolerance = tolerance or 0.1

    local npcs = {}
    for _, npc in mxu.iter(mx.npcs()) do
        table.insert(npcs, npc)
    end

    local separation = 45

    for iter = 1, iterations do
        local totalOverlap = 0

        for i = 1, #npcs do
            local npc1 = npcs[i]
            local pos1 = mx.worldPos(npc1)
            local x1, y1 = pos1.floats[0], pos1.floats[1]
            local r1 = separation

            for j = i + 1, #npcs do
                local npc2 = npcs[j]
                local pos2 = mx.worldPos(npc2)
                local x2, y2 = pos2.floats[0], pos2.floats[1]
                local r2 = separation

                local dx, dy = x2 - x1, y2 - y1
                local distSq = dx * dx + dy * dy

                -- if exactly on top of each other, jitter slightly
                if distSq < 1 then
                    dx = (math.random() - 0.5) * 50
                    dy = (math.random() - 0.5) * 50
                    distSq = dx * dx + dy * dy
                end

                local dist = math.sqrt(distSq)
                local overlap = (r1 + r2) - dist

                if overlap > 0 then
                    totalOverlap = totalOverlap + overlap

                    -- normalize direction safely
                    local nx, ny = dx / dist, dy / dist
                    local push = overlap * 0.5 * 0.8 -- damped push

                    -- If one NPC is fixed, only move the other
                    local moveNpc1 = (npc1 ~= fixedNpc)
                    local moveNpc2 = (npc2 ~= fixedNpc)

                    local newX1, newY1 = x1, y1
                    local newX2, newY2 = x2, y2

                    if moveNpc1 and moveNpc2 then
                        -- Both can move – split the push
                        newX1 = x1 - nx * push
                        newY1 = y1 - ny * push
                        newX2 = x2 + nx * push
                        newY2 = y2 + ny * push
                    elseif moveNpc1 then
                        -- Only npc1 moves (npc2 is fixed)
                        newX1 = x1 - nx * (push * 2)
                        newY1 = y1 - ny * (push * 2)
                    elseif moveNpc2 then
                        -- Only npc2 moves (npc1 is fixed)
                        newX2 = x2 + nx * (push * 2)
                        newY2 = y2 + ny * (push * 2)
                    end

                    if moveNpc1 then
                        local obb1 = mx.worldBounds(npc1)
                        mxu.setObbPos(obb1, newX1, newY1, pos1.floats[2])
                        mx.setLocalBoundsAttr(npc1, "obb", obb1)
                    end
                    if moveNpc2 then
                        local obb2 = mx.worldBounds(npc2)
                        mxu.setObbPos(obb2, newX2, newY2, pos2.floats[2])
                        mx.setLocalBoundsAttr(npc2, "obb", obb2)
                    end

                    -- update positions for subsequent calculations
                    x1, y1 = newX1, newY1
                    x2, y2 = newX2, newY2
                end
            end
        end

        if totalOverlap < tolerance then
            break
        end
    end
end

function turnTo(npc, dst)
    -- Get positions
    local npcPos = mx.worldPos(npc)
    local dstPos = mx.worldPos(dst)
    local x1, y1, z1 = npcPos.floats[0], npcPos.floats[1], npcPos.floats[2]
    local x2, y2, z2 = dstPos.floats[0], dstPos.floats[1], dstPos.floats[2]

    -- Compute direction vector toward destination
    local dx, dy, dz = x2 - x1, y2 - y1, z2 - z1
    local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
    if dist < 1e-4 then
        resolveNpcOverlaps(npc, 4)
        return turnTo(npc, dst)
    end
    local nx, ny, nz = dx / dist, dy / dist, dz / dist

    -- Compute facing orientation (Z rotation)
    local angle = math.atan2(ny, nx)
    local half = angle * 0.5
    local qx, qy, qz, qw = 0, 0, math.sin(half), math.cos(half)

    -- Update OBB rotation
    local npcObb = mx.worldBounds(npc)
    mxu.setObbQuat(npcObb, qx, qy, qz, qw)

    -- Update the environment
    mx.setLocalBoundsAttr(npc, "obb", npcObb)
    return msym.t
end

function walkTo(npc, dst)
    -- Get positions
    local npcPos = mx.worldPos(npc)
    local dstPos = mx.worldPos(dst)
    local x1, y1, z1 = npcPos.floats[0], npcPos.floats[1], npcPos.floats[2]
    local x2, y2, z2 = dstPos.floats[0], dstPos.floats[1], dstPos.floats[2]

    -- Compute direction vector toward destination
    local dx, dy, dz = x2 - x1, y2 - y1, z2 - z1
    local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
    if dist < 75 then
        resolveNpcOverlaps(npc, 4)
        return msym.t -- Arrived
    end

    -- Normalize direction
    local nx, ny, nz = dx / dist, dy / dist, dz / dist

    -- Speed (tunable)
    local speed = 100  -- world units per second

    -- Compute movement this frame
    local moveDist = speed * dt
    if moveDist > dist then
        moveDist = dist  -- prevent overshoot
    end

    -- New position
    local newX = x1 + nx * moveDist
    local newY = y1 + ny * moveDist
    local newZ = z1 + nz * moveDist

    -- Update OBB position
    local npcObb = mx.worldBounds(npc)
    mxu.setObbPos(npcObb, newX, newY, newZ)

    -- Compute facing orientation (Z rotation)
    local angle = math.atan2(ny, nx)

    -- Convert 2D angle to quaternion (rotation around Z axis)
    local half = angle * 0.5
    local qx, qy, qz, qw = 0, 0, math.sin(half), math.cos(half)

    -- Update OBB rotation
    mxu.setObbQuat(npcObb, qx, qy, qz, qw)

    -- Update the environment
    mx.setLocalBoundsAttr(npc, "obb", npcObb)
    return msym.f -- not there yet
end
