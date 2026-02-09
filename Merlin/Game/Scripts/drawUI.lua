

require("drawUtil")

function printSelectedNpcSelfKnowledge()
    mx.dumpSelfKnowledge()
end

local function insertSection(strings, title, color)
    table.insert(strings, {text = title, color = color, isHeader = true})
end

function drawNpcStatePanel(xRight)
    if not showNpcStates or selectedNpc == nil then return end

    local states = mx.ongoingSelfStates(selectedNpc)
    local font = getFont(12)

    local lines = {
        {text = "States", color = blue, isHeader = true}
    }

    for _, state in mxu.iter(states) do
        local txt = nlPrint and mxu.toNlLuaString(state) or mxu.toLuaString(state)
        table.insert(lines, {text = txt, color = blue})
    end

    drawTextPanel{
        x = xRight,
        y = 40,
        anchor = "topright",
        align = "left",
        text = lines,
        font = font,
        bgColor = {0.1, 0.1, 0.2, 0.85},
        borderColor = {1, 1, 1, 0.3},
        textColor = {1, 1, 1, 1},
        padding = 20
    }

    -- Return width so caller knows how much space was used
    --  Estimate: longest line width + padding + border
    local maxWidth = 0
    for _, line in ipairs(lines) do
        local w = font:getWidth(line.text)
        if w > maxWidth then maxWidth = w end
    end
    return maxWidth + 40  -- padding × 2
end

function drawSelectedNpcInfo()
    -- Determine info panel X-position
    local screenW, _ = love.graphics.getDimensions()
    local infoRight = screenW - 20

    -- If states panel is enabled, draw it first and shift the info panel left
    if showNpcStates then
        local usedWidth = drawNpcStatePanel(infoRight)
        if usedWidth ~= nil then
            infoRight = infoRight - usedWidth - 20
        end
    end

    if not showNpcInfo or selectedNpc == nil then return end

    local ruleNames = mx.firingRuleNames(selectedNpc)
    local goals = mx.currentGoals(selectedNpc)
    local ptasks = mx.proposedTasks(selectedNpc)
    local rtasks = mx.runningTasks(selectedNpc)
    local pactions = mx.proposedActions(selectedNpc)
    local ractions = mx.runningActions(selectedNpc)
    local states = mx.ongoingSelfStates(selectedNpc)

    local strings = {}

    if crtText ~= nil then
        insertSection(strings, "Query Results", white)
        table.insert(strings, {text = crtText, color=white})
        local results = mx.every(crtText)
        for _, pattern in mxu.iter(results) do
            local txt = nlPrint and mxu.toNlLuaString(pattern) or mxu.toLuaString(pattern)
            table.insert(strings, {text = txt, color=white})
        end
        table.insert(strings, {text = "", color=white})
    end

    -- Rules
    insertSection(strings, "Rules", yellow)
    for _, ruleName in mxu.iter(ruleNames) do
        table.insert(strings, {text = mxu.toLuaString(ruleName), color=yellow})
    end

    -- Goals
    insertSection(strings, "Goals", orange)
    for _, goal in mxu.iter(goals) do
        local txt = nlPrint and mxu.toNlLuaString(goal) or mxu.toLuaString(goal)
        table.insert(strings, {text = txt, color=orange})
    end

    -- Tasks
    insertSection(strings, "Proposed Tasks", cyan)
    for i, task, util, outcompetedBy in mxu.actIter(ptasks) do
        local n = 0
        if showCauses then
            -- Number of causes for this act
            n = ptasks.numCauses[i-1]
            -- Print each cause, increasing indent
            for c = 0, n-1 do
                local causeSym = ptasks.causes[i-1][c]     -- raw symbol
                local causeTxt = nlPrint and mxu.toNlLuaString(causeSym)
                                          or mxu.toLuaString(causeSym)
                -- Indent grows with cause depth
                local indent = string.rep("  ", c)
                table.insert(strings, {
                    text  = indent .. causeTxt,
                    color = blue
                })
            end
        end
        -- Print the act itself
        local actIndent = string.rep("  ", n)
        local actTxt = nlPrint and mxu.toNlLuaString(task)
                               or mxu.toLuaString(task)
        table.insert(strings, {
            text  = actIndent .. "u" .. util .. ": " .. actTxt,
            color = cyan
        })
        if outcompetedBy ~= 0 then
            local actIndent = string.rep("  ", n)
            local outcompetedTxt = nlPrint and mxu.toNlLuaString(outcompetedBy) or mxu.toLuaString(outcompetedBy)
            table.insert(strings, {text = actIndent .. "(outcompeted by: " .. outcompetedTxt .. ")", color=blue})
        end
    end
    insertSection(strings, "Running Tasks", green)
    for i, task, util, _ in mxu.actIter(rtasks) do
        local n = 0
        if showCauses then
            -- Number of causes for this act
            n = rtasks.numCauses[i-1]
            -- Print each cause, increasing indent
            for c = 0, n-1 do
                local causeSym = rtasks.causes[i-1][c]     -- raw symbol
                local causeTxt = nlPrint and mxu.toNlLuaString(causeSym)
                                          or mxu.toLuaString(causeSym)
                -- Indent grows with cause depth
                local indent = string.rep("  ", c)
                table.insert(strings, {
                    text  = indent .. causeTxt,
                    color = green
                })
            end
        end
        -- Print the act itself
        local actIndent = string.rep("  ", n)
        local actTxt = nlPrint and mxu.toNlLuaString(task)
                               or mxu.toLuaString(task)
        table.insert(strings, {
            text  = actIndent .. "u" .. util .. ": " .. actTxt,
            color = showCauses and brightGreen or green
        })
    end

    -- Actions
    insertSection(strings, "Proposed Actions", pink)
     for _, action, util, _ in mxu.actIter(pactions) do
         local txt = nlPrint and mxu.toNlLuaString(action) or mxu.toLuaString(action)
         table.insert(strings, {text = util .. ": " .. txt, color=pink})
     end
    insertSection(strings, "Running Actions", red)
    for _, action, util, _ in mxu.actIter(ractions) do
        local txt = nlPrint and mxu.toNlLuaString(action) or mxu.toLuaString(action)
        table.insert(strings, {text = util .. ": " .. txt, color=red})
    end

    local font = getFont(12)
    local screenW, _ = love.graphics.getDimensions()
    local xOffset = 20

    -- Assemble the lines for text-panel drawing
    local lines = {}
    table.insert(lines, {text = "NPC: " .. mxu.toLuaString(selectedNpc), color = white, isHeader = true})
    for _, s in ipairs(strings) do
        table.insert(lines, s)
    end

    -- Draw main info panel
    drawTextPanel{
        x = infoRight,
        y = 40,
        anchor = "topright",
        align = "left",
        text = lines,
        font = font,
        bgColor = {0.1, 0.1, 0.1, 0.85},
        borderColor = {1, 1, 1, 0.3},
        textColor = {1, 1, 1, 1},
        padding = 20
    }
end

function drawCrossHair()
    love.graphics.setColor(1, 0.2, 0.2, 1)
    love.graphics.setLineWidth(2)
    love.graphics.line(mouseX - 10, mouseY, mouseX + 10, mouseY)
    love.graphics.line(mouseX, mouseY - 10, mouseX, mouseY + 10)
    love.graphics.setColor(1, 1, 1, 1)
end

function drawHUD()
    local font = getFont(18)
    love.graphics.setFont(font)

    local lookAtX, lookAtY = screenToWorld(mouseX, mouseY)
    local zoomText   = string.format("Zoom: %.2f", zoom)
    local lookAtText = "Look-at: " .. string.format("%.0f, %.0f", lookAtX, lookAtY)
    local cycleText  = "Cycle: " .. mx.cycle()

    local dateText   = "Date: " .. mxu.toLuaString(mx.dateSymbol())
    local timeText   = "Time: " .. mxu.toLuaString(mx.secondsSymbol())
    local popText    = "Pop: " .. mx.npcs().size

    local startY = 36
    local padding = 10
    local lineH = font:getHeight() + 2
    local xOffset = 20  -- ← left margin

    -- Box 1: Simulation Info (LHS)
    local simLines = {"Simulation", zoomText, lookAtText, cycleText}
    drawTextPanel{
        x = xOffset,
        y = startY + 50,
        text = simLines,
        font = font,
        anchor = "topleft",  -- ← now pinned to the left
        align = "left",
        bgColor = {0.18, 0.2, 0.23, 0.85},
        borderColor = {1, 1, 1, 0.15},
        textColor = {1, 1, 1, 1},
        padding = padding
    }

    -- Estimate height for stacking below
    local box1H = (#simLines + 1) * lineH + padding * 2 + 8

    -- Box 2: World Info
    drawTextPanel{
        x = xOffset,
        y = startY + box1H + 50,
        text = {"World", dateText, timeText, popText},
        font = font,
        anchor = "topleft",  -- ← same here
        align = "left",
        bgColor = {0.15, 0.25, 0.22, 0.85},
        borderColor = {1, 1, 1, 0.15},
        textColor = {1, 1, 1, 1},
        padding = padding
    }
end

function drawPopup()
    if not popupActive then return end
    local w, h = love.graphics.getDimensions()
    local panelW, panelH = 600, 160
    local x, y = (w - panelW) / 2, (h - panelH) / 2

    drawTextPanel{
        x = x + panelW / 2,
        y = y + 100,
        text = popupText .. "|",
        font = getFont(18),
        bgColor = {0.1, 0.1, 0.1, 1},
        borderColor = {1, 1, 1, 1},
        textColor = {1, 1, 1, 1},
        anchor = "center"
    }
end

function drawStatusBar()
    local w, h = love.graphics.getDimensions()
    local barHeight = 32

    -- Background
    love.graphics.setColor(0.12, 0.12, 0.12, 0.9)
    love.graphics.rectangle("fill", 0, 0, w, barHeight)

    -- Border
    love.graphics.setColor(1, 1, 1, 0.1)
    love.graphics.rectangle("line", 0, 0, w, barHeight)

    local items = {
        {label = "1:Spaces",        enabled = drawSpacesFlag, color = white},
        {label = "2:Sounds",        enabled = drawSoundsFlag, color = white},
        {label = "3:SoundBubbles",  enabled = drawSoundBubblesFlag, color = white},
        {label = "4:Sectors",       enabled = drawSectorsFlag, color = white},
        {label = "5:Terrain",       enabled = drawTerrainFlag, color = white},
        {label = "6:Tasks",         enabled = drawTasksFlag, color = white}
    }
    if selectedNpc ~= nil then
        table.insert(items, {label = "i:Info", enabled = showNpcInfo, color = white})
        table.insert(items, {label = "s:States", enabled = showNpcStates, color = white})
        table.insert(items, {label = "h:Cause Hierarchy", enabled = showCauses, color = white})
        table.insert(items, {label = "e:Draw Env", enabled = drawEnv, color = white})
        table.insert(items, {label = "m:Draw Mental", enabled = drawMental, color = white})
    end

    local x = 12
    local y = 6
    local font = getFont(18)
    love.graphics.setFont(font)

    for _, item in ipairs(items) do
        local col = item.color
        local label = item.label

        if not item.enabled then
            -- Desaturate + dim when disabled
            love.graphics.setColor(col[1] * 0.5, col[2] * 0.5, col[3] * 0.5, 0.7)
            label = label .. " (off)"
        else
            love.graphics.setColor(col[1], col[2], col[3], 1.0)
        end

        love.graphics.print(label, x, y)
        x = x + font:getWidth(label) + 32
    end

    -- Reset color
    love.graphics.setColor(1, 1, 1, 1)
end

function drawTimeStepBanner()
    local screenW, _ = love.graphics.getDimensions()
    local barHeight = 32       -- height of the mini bar
    local yOffset = 32         -- immediately below main status bar
    local font = getFont(18)

    local label
    if timeStepMode == realtime then
        label = "realtime"
    end
    if timeStepMode == debug then
        label = "debugtime"
    end
    if timeStepMode == historical then
        label = "historical"
    end

    drawTextPanel{
        x = 0,
        y = yOffset,
        width = screenW,       -- full screen width
        height = barHeight,    -- fixed height
        text = {label},
        font = font,
        bgColor = {0.2, 1.0, 0.4, 0.35},   -- transparent green
        borderColor = {0, 0, 0, 0},        -- no border
        textColor = {1, 1, 1, 1},
        padding = 0,
        align = "center",
        anchor = "topleft",
        forceWidth = true      -- NEW: forces the background rectangle to fill `width`
    }
end

function drawPauseBanner()
    if stepSim then return end

    local screenW, screenH = love.graphics.getDimensions()
    drawTextPanel{
        x = screenW / 2,
        y = screenH - 80,
        text = "SIMULATION PAUSED",
        font = getFont(48),
        bgColor = {1, 0.85, 0.2, 0.35},
        borderColor = {1, 0.85, 0.2, 0.8},
        textColor = {1, 0.85, 0.2, 1},
        anchor = "center"
    }
end

