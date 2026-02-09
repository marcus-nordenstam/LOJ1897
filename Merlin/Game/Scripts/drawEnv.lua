
require("drawUtil")

-- Minimum screen-space area (in pixels^2) required to draw an entity
local MIN_SCREEN_AREA = 256

-- Maximum screen-space area (in pixels^2) for the label to be shown
local MAX_SCREEN_AREA_LABEL_DRAW = 500*500

-- Shoelace formula to compute polygon area. Expects corners as {{x,y},...}
local function screenPolygonArea(corners)
    local n = #corners
    if n < 3 then return 0 end
    local sum = 0
    for i = 1, n do
        local x1, y1 = corners[i][1], corners[i][2]
        local j = (i % n) + 1
        local x2, y2 = corners[j][1], corners[j][2]
        sum = sum + (x1 * y2 - x2 * y1)
    end
    return math.abs(sum) * 0.5
end

-- Draws a +X arrow for the given OBB
local function drawObbXAxisArrow(obb, color, length)
    length = length or 20 -- default arrow length

    -- OBB center
    local wx, wy = obb.floats[0], obb.floats[1]

    -- Quaternion → rotation angle
    local qx, qy, qz, qw = obb.floats[6], obb.floats[7], obb.floats[8], obb.floats[9]
    local angle = quatToAngle(qx, qy, qz, qw)

    -- Compute +X direction in world space
    local dx, dy = math.cos(angle), math.sin(angle)

    -- Arrow tip in world space
    local tipX, tipY = wx + dx * length, wy + dy * length

    -- Convert to screen space
    local sx, sy = worldToScreen(wx, wy)
    local tipSx, tipSy = worldToScreen(tipX, tipY)

    -- Draw line
    love.graphics.setColor(color[1], color[2], color[3], color[4] or 1)
    love.graphics.setLineWidth(2)
    love.graphics.line(sx, sy, tipSx, tipSy)

    -- Draw arrowhead
    local arrowSize = 6
    local angleOffset = math.pi / 6 -- 30 degrees
    local leftX = tipSx - arrowSize * math.cos(angle - angleOffset)
    local leftY = tipSy - arrowSize * math.sin(angle - angleOffset)
    local rightX = tipSx - arrowSize * math.cos(angle + angleOffset)
    local rightY = tipSy - arrowSize * math.sin(angle + angleOffset)

    love.graphics.line(tipSx, tipSy, leftX, leftY)
    love.graphics.line(tipSx, tipSy, rightX, rightY)
end

-- Converts an OBB into 4 rotated world-space corners
function obbWorldCorners(obb)
    local wx, wy = obb.floats[0], obb.floats[1]
    local dx, dy = obb.floats[3], obb.floats[4]
    local qx, qy, qz, qw = obb.floats[6], obb.floats[7], obb.floats[8], obb.floats[9]

    local angle = quatToAngle(qx, qy, qz, qw)
    local hx, hy = dx / 2, dy / 2

    local localCorners = {
        {-hx, -hy},
        { hx, -hy},
        { hx,  hy},
        {-hx,  hy}
    }

    local worldCorners = {}
    local cosA, sinA = math.cos(angle), math.sin(angle)
    for i, c in ipairs(localCorners) do
        local lx, ly = c[1], c[2]
        local rx = wx + lx * cosA - ly * sinA
        local ry = wy + lx * sinA + ly * cosA
        table.insert(worldCorners, {rx, ry})
    end
    return worldCorners
end

-- Converts world corners to screen-space
function toScreenCorners(worldCorners)
    local screenCorners = {}
    for _, wc in ipairs(worldCorners) do
        local sx, sy = worldToScreen(wc[1], wc[2])
        table.insert(screenCorners, {sx, sy})
    end
    return screenCorners
end

local function drawSector(min, max, color)
    -- Draw the outer sector boundary
    local worldCorners = {
        { min.floats[0], min.floats[1] },
        { max.floats[0], min.floats[1] },
        { max.floats[0], max.floats[1] },
        { min.floats[0], max.floats[1] }
    }
    local screenCorners = toScreenCorners(worldCorners)
    love.graphics.setColor(color[1], color[2], color[3], color[4] or 1)
    love.graphics.setLineWidth(1)
    love.graphics.polygon("line",
        screenCorners[1][1], screenCorners[1][2],
        screenCorners[2][1], screenCorners[2][2],
        screenCorners[3][1], screenCorners[3][2],
        screenCorners[4][1], screenCorners[4][2]
    )

    --------------------------------------------------
    -- Draw heightfield as color-coded lattice (16x16)
    --------------------------------------------------
    if not drawTerrainFlag then return end

    local steps = 16
    local dx = (max.floats[0] - min.floats[0]) / (steps - 1)
    local dy = (max.floats[1] - min.floats[1]) / (steps - 1)

    -- First, find min/max height in this sector
    local hmin, hmax = math.huge, -math.huge
    local heights = {}

    for iy = 0, steps - 1 do
        for ix = 0, steps - 1 do
            local wx = min.floats[0] + ix * dx
            local wy = min.floats[1] + iy * dy
            local h = mx.terrainElevation(wx, wy)
            heights[#heights + 1] = { wx, wy, h }
            if h < hmin then hmin = h end
            if h > hmax then hmax = h end
        end
    end

    -- Draw colored points (or tiny quads) for each sample
    for _, p in ipairs(heights) do
        local wx, wy, h = p[1], p[2], p[3]
        local t = (h - hmin) / math.max(1e-6, hmax - hmin)

        -- Interpolate color: green -> yellow -> red
        --  t=0 : green (0,1,0)
        --  t=0.5 : yellow (1,1,0)
        --  t=1 : red (1,0,0)
        local r, g, b
        if t < 0.5 then
            -- Green -> Yellow
            r = t * 2
            g = 1.0
            b = 0.0
        else
            -- Yellow -> Red
            r = 1.0
            g = 1.0 - (t - 0.5) * 2
            b = 0.0
        end

        local sx, sy = worldToScreen(wx, wy)
        love.graphics.setColor(r, g, b, 1.0)
        love.graphics.rectangle("fill", sx - 1, sy - 1, 2, 2)
    end
end


function drawSectors()
    if not drawSectorsFlag then return end
    local sectorColor
    if selectedNpc ~= nil then
        sectorColor = grey
    else
        sectorColor = white
    end
    for _, min, max in mxu.sectorIter(sectors) do
        drawSector(min, max, sectorColor)
    end
    if selectedNpc ~= nil then
        local visibleSectors = mx.visibleSectors()
        for _, min, max in mxu.sectorIter(visibleSectors) do
            drawSector(min, max, brightYellow)
        end
    end
end

-- Draws an oriented box outline
local function drawObbOutline(screenCorners, color)
    love.graphics.setColor(color[1], color[2], color[3], color[4] or 1)
    love.graphics.setLineWidth(1)
    love.graphics.polygon("line",
        screenCorners[1][1], screenCorners[1][2],
        screenCorners[2][1], screenCorners[2][2],
        screenCorners[3][1], screenCorners[3][2],
        screenCorners[4][1], screenCorners[4][2]
    )
    love.graphics.setColor(color[1], color[2], color[3], 0.1)
    love.graphics.polygon("fill",
        screenCorners[1][1], screenCorners[1][2],
        screenCorners[2][1], screenCorners[2][2],
        screenCorners[3][1], screenCorners[3][2],
        screenCorners[4][1], screenCorners[4][2]
    )
end

local function drawEntObb(obb, label, color)
    -- Draw the OBB
    local worldCorners = obbWorldCorners(obb)
    local screenCorners = toScreenCorners(worldCorners)
    local area = screenPolygonArea(screenCorners)
    if area < MIN_SCREEN_AREA then
        -- too small on screen; skip drawing entirely
        return
    end
    drawObbOutline(screenCorners, color)
    -- Draw label
    if area < MAX_SCREEN_AREA_LABEL_DRAW then
        local wx, wy = obb.floats[0], obb.floats[1]
        local sx, sy = worldToScreen(wx, wy)
        local font = zoomFont()
        local prevFont = love.graphics.getFont()
        love.graphics.setFont(font)
        local textW = font:getWidth(label)
        local textH = font:getHeight()
        love.graphics.setColor(color[1], color[2], color[3], color[4])
        love.graphics.print(label, sx - textW / 2, sy - textH / 2)
    end
end

local function drawMentalEnt(ent, color)
    local obb = mx.mentalBounds(selectedNpc, ent)
    if mxu.isValidBounds(obb) then
        drawEntObb(obb, mxu.toLuaString(ent), brightYellow)
    end
end

local function drawEnvEnt(ent, color)
    drawEntObb(mx.worldBounds(ent), mxu.toLuaString(ent), color)
end

function drawBuildings()
    if selectedNpc ~= nil and drawMental then
        local mentities = mx.mentalObjectsOfKind("building");
        for _, ent in mxu.iter(mentities) do
            drawMentalEnt(ent, brightYellow)
        end
    end

    if drawEnv or selectedNpc == nil then
        for _, ent in ipairs(allBuildings) do
            drawEnvEnt(ent, bldgColor)
        end
    end
end

function drawSpaces()
    if not drawSpacesFlag then return end

    if selectedNpc ~= nil and drawMental then
        local mentities = mx.mentalObjectsOfKind("space");
        for _, ent in mxu.iter(mentities) do
            drawMentalEnt(ent, brightYellow)
        end
    end

    if drawEnv or selectedNpc == nil then
        for _, ent in mxu.iter(spaces) do
            drawEnvEnt(ent, spaceColor)
        end
    end
end

function drawProps()
    if selectedNpc ~= nil and drawMental then
        local mentities = mx.mentalObjectsOfKind("ring");
        for _, ent in mxu.iter(mentities) do
            drawMentalEnt(ent, brightYellow)
        end
    end

    if drawEnv or selectedNpc == nil then
        local rings = mx.findEntitiesOfKind("prop", "ring")
        for _, ent in mxu.iter(rings) do
            drawEnvEnt(ent, propColor)
        end
    end
end

local function drawFacingObb(worldCorners, screenCorners, color)
    -- Front edge along local +X: corners with largest local X
    -- In obbWorldCorners(), localCorners are:
    -- 1: -hx,-hy   2: hx,-hy   3: hx,hy   4: -hx,hy
    local i1, i2 = 2, 3 -- front-left and front-right in worldCorners

    local x1, y1 = screenCorners[i1][1], screenCorners[i1][2]
    local x2, y2 = screenCorners[i2][1], screenCorners[i2][2]

    -- Center
    local cx, cy = 0, 0
    for i = 1, 4 do
        cx = cx + screenCorners[i][1]
        cy = cy + screenCorners[i][2]
    end
    cx, cy = cx / 4, cy / 4

    -- Midpoint
    local mx, my = (x1 + x2)/2, (y1 + y2)/2

    -- Perpendicular to front edge
    local dx, dy = x2 - x1, y2 - y1
    local len = math.sqrt(dx*dx + dy*dy)
    local px, py = -dy/len, dx/len

    -- Ensure perpendicular points outward
    local vx, vy = mx - cx, my - cy
    if (px*vx + py*vy) < 0 then px, py = -px, -py end

    -- Scale nose based on zoom
    local baseNoseLength = 8
    local noseLength = baseNoseLength / zoom
    local nx, ny = mx + px*noseLength, my + py*noseLength

    -- Build pentagon
    local verts = {}
    for i = 1, 4 do
        if i == i1 then
            table.insert(verts, x1); table.insert(verts, y1)
            table.insert(verts, nx); table.insert(verts, ny)
            table.insert(verts, x2); table.insert(verts, y2)
        elseif i ~= i2 then
            table.insert(verts, screenCorners[i][1])
            table.insert(verts, screenCorners[i][2])
        end
    end

    -- Outline
    love.graphics.setColor(color[1], color[2], color[3], color[4] or 1)
    love.graphics.setLineWidth(1)
    love.graphics.polygon("line", verts)

    -- Fill
    love.graphics.setColor(color[1], color[2], color[3], 0.1)
    love.graphics.polygon("fill", verts)
end

function drawNpcLabel(npc, obb, label, color)
    local font = zoomFont()
    love.graphics.setFont(font)
    local textW = font:getWidth(label)
    local textH = font:getHeight()
    local wx, wy = obb.floats[0], obb.floats[1]
    local sx, sy = worldToScreen(wx, wy)
    local lx, ly = sx + 10, sy

    if selectedNpc == npc then
        love.graphics.setColor(color)
        love.graphics.rectangle("fill", lx - 2, ly - 2, textW + 4, textH + 4, 4, 4)
        love.graphics.setColor(0, 0, 0, 1)
        love.graphics.print(label, lx, ly)
    else
        love.graphics.setColor(color)
        love.graphics.print(label, lx, ly)
    end
end

function drawNpc(npc, obb, label, color)
    if not mxu.isValidBounds(obb) then return end
    local qx, qy, qz, qw = obb.floats[6], obb.floats[7], obb.floats[8], obb.floats[9]
    local angle = quatToAngle(qx, qy, qz, qw)
    local worldCorners = obbWorldCorners(obb)
    local screenCorners = toScreenCorners(worldCorners)
    local convoBelief = mx.anyPresentTensePossessiveBeliefTarget(npc, msym.conversation)
    if convoBelief ~= msym.nothing and convoBelief ~= msym.fail then
        local participants = mx.everyPresentTensePossessiveBeliefTargets(convoBelief, msym.participant)
        local num = participants.size
        if num > 0 then
            -- Collect screen positions of all participants
            local points = {}
            for _, p in mxu.iter(participants) do
                local obb = mx.mentalBounds(npc, p)
                if mxu.isValidBounds(obb) then
                    local pos = mxu.obbPos(obb)
                    local wx, wy = pos.floats[0], pos.floats[1]
                    local sx, sy = worldToScreen(wx, wy)
                    table.insert(points, {sx, sy})
                end
            end

            if #points > 0 then
                local baseRadius = 50
                local baseLineWidth = 100
                local radius = baseRadius / zoom
                local lineWidth = baseLineWidth / zoom

                love.graphics.stencil(function()
                    -- Draw all primitives that define the shape
                    if #points == 1 then
                        love.graphics.circle("fill", points[1][1], points[1][2], radius)
                    elseif #points == 2 then
                        local sx1, sy1 = points[1][1], points[1][2]
                        local sx2, sy2 = points[2][1], points[2][2]
                        love.graphics.setLineWidth(lineWidth)
                        love.graphics.line(sx1, sy1, sx2, sy2)
                        love.graphics.circle("fill", sx1, sy1, radius)
                        love.graphics.circle("fill", sx2, sy2, radius)
                    else
                        local hull = convexHull(points)
                        if #hull >= 3 then
                            local flat = {}
                            for _, pt in ipairs(hull) do
                                table.insert(flat, pt[1])
                                table.insert(flat, pt[2])
                            end
                            love.graphics.polygon("fill", flat)
                        elseif #hull == 2 then
                            local sx1, sy1 = hull[1][1], hull[1][2]
                            local sx2, sy2 = hull[2][1], hull[2][2]
                            love.graphics.setLineWidth(lineWidth)
                            love.graphics.line(sx1, sy1, sx2, sy2)
                            love.graphics.circle("fill", sx1, sy1, radius)
                            love.graphics.circle("fill", sx2, sy2, radius)
                        elseif #hull == 1 then
                            local sx, sy = hull[1][1], hull[1][2]
                            love.graphics.circle("fill", sx, sy, radius)
                        end
                    end
                end, "replace", 1)

                love.graphics.setStencilTest("greater", 0)
                love.graphics.setColor(green[1], green[2], green[3], 0.3)
                love.graphics.rectangle("fill", 0, 0, love.graphics.getWidth(), love.graphics.getHeight())
                love.graphics.setStencilTest()
            end
        end
    end

    -- Draw the NPC
    drawFacingObb(worldCorners, screenCorners, color, angle)
    local area = screenPolygonArea(screenCorners)
    if area < MAX_SCREEN_AREA_LABEL_DRAW then
        drawNpcLabel(npc, obb, label, color)
    end
end

function drawNpcs()
    if selectedNpc ~= nil and drawMental then
        local mentities = mx.mentalObjectsOfKind("human");
        for _, ent in mxu.iter(mentities) do
            local label = tostring(mxu.toLuaString(ent) or "")
            drawNpc(ent, mx.mentalBounds(selectedNpc, ent), label, brightYellow)
        end
        local hands = mx.mentalObjectsOfKind("object hand")
        for _, hand in mxu.iter(hands) do
            drawMentalEnt(hand, brightYellow)
        end
        local fingers = mx.mentalObjectsOfKind("object finger")
        for _, finger in mxu.iter(fingers) do
            drawMentalEnt(finger, brightYellow)
        end
        -- Highlight the selected NPC
        drawNpcLabel(selectedNpc, mx.worldBounds(selectedNpc), mxu.toLuaString(selectedNpc), brightYellow)
    end

    if drawEnv or selectedNpc == nil then
        for _, npc in mxu.iter(mx.npcs()) do
            local label = tostring(mxu.toLuaString(npc) or "")
            drawNpc(npc, mx.worldBounds(npc), label, npcColor(npc))
        end
        local hands = mx.findEntitiesOfKind("hand", "object hand")
        for _, hand in mxu.iter(hands) do
            local npc = mx.attrSymbol(hand, "parent")
            drawEnvEnt(hand, npcColor(npc))
        end
        local fingers = mx.findEntitiesOfKind("finger", "object finger")
        for _, finger in mxu.iter(fingers) do
            local hand = mx.attrSymbol(finger, "parent")
            local npc = mx.attrSymbol(hand, "parent")
            drawEnvEnt(finger, npcColor(npc))
        end
    end

    love.graphics.setColor(1, 1, 1, 1)
end

function drawRunningTasks()
    if not drawTasksFlag then return end
    if not drawEnv then return end

    local now = love.timer.getTime()
    local font = zoomFont()
    love.graphics.setFont(font)
    local padding = 8
    local lineHeight = font:getHeight() + 2

    -- Group by NPC
    local groups = {}

    for _, npc in mxu.iter(mx.npcs()) do
        local tasks = mx.runningTasks(npc)
        if tasks and tasks.size > 0 then
            local key = tostring(npc)
            groups[key] = {
                npc = npc,
                tasks = {}
            }

            for _, t in mxu.actIter(tasks) do
                table.insert(groups[key].tasks, t)
            end
        end
    end

    -- Convert groups to array, sort by NPC ID for stability
    local arr = {}
    for _, g in pairs(groups) do table.insert(arr, g) end
    table.sort(arr, function(a, b)
        return tonumber(a.npc) < tonumber(b.npc)
    end)

    -- Draw stacks
    for _, g in ipairs(arr) do
        local obb = mx.worldBounds(g.npc)
        local pos = mxu.obbPos(obb)
        local wx, wy = pos.floats[0], pos.floats[1]
        local sx, sy = worldToScreen(wx, wy)
        sy = sy - 20
        for _, task in ipairs(g.tasks) do
            local msg = mxu.toNlLuaString(task)
            local lines = wrapTextToWidth(msg, font, 360)
            local _, h = drawTextPanel{
                x = sx,
                y = sy,
                text = lines,
                font = font,
                bgColor = {0.08, 0.08, 0.08, 0.9},
                borderColor = {1, 1, 1, 0.18},
                textColor = {blue[1], blue[2], blue[3], alpha},
                anchor = "bottomleft",
                padding = padding
            }

            sy = sy - (h + 4)   -- stack upward
        end
    end
end

function drawSoundBubbles()
    if #soundBubbles == 0 or not drawSoundBubblesFlag then return end
    if not drawEnv then return end

    local now = love.timer.getTime()
    local screenW, screenH = love.graphics.getDimensions()
    local font = zoomFont()
    local prevFont = love.graphics.getFont()
    love.graphics.setFont(font)

    local lineHeight = font:getHeight() + 2
    local padding = 8

    -- Age each bubble
    for i = #soundBubbles, 1, -1 do
        local sb = soundBubbles[i]
        if not stepSim then
            sb.age = sb.pausedAge -- use paused age
        else
            sb.age = now - sb.t
        end
    end

    -- Draw each bubble only once per position
    if drawSoundsFlag then
        local drawnPositions = {}
        for _, sb in ipairs(soundBubbles) do
            if sb.age ~= nil and sb.age > sb.life then
                sb._expired = true
            else
                local alpha = math.max(1 - sb.age / sb.life, 0.05)
                local wx, wy = sb.pos.floats[0], sb.pos.floats[1]
                local sx, sy = worldToScreen(wx, wy)
                local key = string.format("%d_%d", math.floor(sx), math.floor(sy))
                if not drawnPositions[key] then
                    drawnPositions[key] = true
                    local radiusScreen = sb.rad / zoom
                    love.graphics.setLineWidth(1)
                    love.graphics.setColor(1, 1, 0.3, 0.1 * alpha)
                    love.graphics.circle("fill", sx, sy, radiusScreen)
                    love.graphics.setColor(1, 1, 0.3, 0.4 * alpha)
                    love.graphics.circle("line", sx, sy, radiusScreen)
                end
            end
        end
    end

    local groups = {}
    for _, sb in ipairs(soundBubbles) do
        local key = tostring(sb.speaker)  -- safe Lua table key
        if not groups[key] then
            groups[key] = {
                speaker = sb.speaker,  -- store original uint64_t for MX calls
                bubbles = {}
            }
        end
        table.insert(groups[key].bubbles, sb)
    end

    -- Convert groups table into a stable array for drawing
    local groupArray = {}
    for _, groupData in pairs(groups) do
        table.insert(groupArray, groupData)
    end

    -- Sort by speaker ID (or any stable criterion)
    table.sort(groupArray, function(a, b)
        return tonumber(a.speaker) < tonumber(b.speaker)
    end)

    -- Draw text panels stacked properly
    for _, groupData in ipairs(groupArray) do
        local speaker = groupData.speaker      -- original uint64_t
        local group = groupData.bubbles        -- list of bubbles
        local obb = mx.worldBounds(speaker)
        local pos = mxu.obbPos(obb)
        local wx, wy = pos.floats[0], pos.floats[1]
        local sx, sy = worldToScreen(wx, wy)
        sy = sy + 20 -- initial offset
        for _, sb in ipairs(group) do
            if sb.age ~= nil and sb.age <= sb.life then
                local alpha = math.max(1 - sb.age / sb.life, 0.05)
                local lines = wrapTextToWidth(sb.msg, font, 360)
                local _, panelHeight = drawTextPanel{
                    x = sx,
                    y = sy,
                    text = lines,
                    font = font,
                    bgColor = {0.08, 0.08, 0.08, 0.9 * alpha},
                    borderColor = {1, 1, 1, 0.18 * alpha},
                    textColor = {yellow[1], yellow[2], yellow[3], alpha},
                    anchor = "topleft",
                    padding = padding
                }
                sy = sy + panelHeight + 4
            end
        end
    end

    -- Clean up expired bubbles
    if stepSim then
        for i = #soundBubbles, 1, -1 do
            if soundBubbles[i]._expired then
                table.remove(soundBubbles, i)
            end
        end
    end

    love.graphics.setFont(prevFont)
end

