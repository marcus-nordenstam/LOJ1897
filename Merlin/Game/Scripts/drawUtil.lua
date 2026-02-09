
require("globals")

-- Extracts quaternion rotation (Z-axis) as 2D angle
function quatToAngle(qx, qy, qz, qw)
    return math.atan2(2 * (qw * qz + qx * qy), 1 - 2 * (qy * qy + qz * qz))
end

function getFont(size)
    -- Cache fonts by size to avoid performance cost
    if not fontCache[size] then
        fontCache[size] = love.graphics.newFont(size)
    end
    return fontCache[size]
end

function zoomFont()
    local baseSize = 10
    local scale = 24 / zoom   -- inverse
    local size = math.floor(baseSize * scale + 0.5)
    size = math.max(8, math.min(16, size))  -- clamp to reasonable range
    return getFont(size)
end

local function rgbToHsv(r, g, b)
    local maxc = math.max(r, g, b)
    local minc = math.min(r, g, b)
    local delta = maxc - minc

    local h = 0
    if delta ~= 0 then
        if maxc == r then
            h = ((g - b) / delta) % 6
        elseif maxc == g then
            h = (b - r) / delta + 2
        else
            h = (r - g) / delta + 4
        end
        h = h / 6
    end

    local s = maxc == 0 and 0 or delta / maxc
    local v = maxc

    return h, s, v
end

local function hsvToRgb(h, s, v)
    local i = math.floor(h * 6)
    local f = h * 6 - i
    local p = v * (1 - s)
    local q = v * (1 - f * s)
    local t = v * (1 - (1 - f) * s)

    i = i % 6
    if i == 0 then return v, t, p
    elseif i == 1 then return q, v, p
    elseif i == 2 then return p, v, t
    elseif i == 3 then return p, q, v
    elseif i == 4 then return t, p, v
    else return v, p, q end
end

function npcColor(npc)
    local str = mxu.toLuaString(npc)
    local hash = 0
    for i = 1, #str do
        hash = (hash * 31 + str:byte(i)) % 0xFFFFFF
    end

    local r = bit.band(bit.rshift(hash, 16), 0xFF) / 255
    local g = bit.band(bit.rshift(hash, 8), 0xFF) / 255
    local b = bit.band(hash, 0xFF) / 255

    -- Convert to HSV
    local h, s, v = rgbToHsv(r, g, b)

    -- Enforce brightness and saturation (tweak as needed)
    s = math.max(s, 0.6)     -- avoid dull/gray colors
    v = math.max(v, 0.75)    -- ensure bright enough for dark backgrounds

    r, g, b = hsvToRgb(h, s, v)

    return {r, g, b, 1}
end

function worldToScreen(wx, wy)
    local w, h = love.graphics.getDimensions()
    local sx = (wx - camX) / zoom + w / 2
    local sy = (wy - camY) / zoom + h / 2
    return sx, sy
end

function screenToWorld(sx, sy)
    local w, h = love.graphics.getDimensions()
    local wx = (sx - w / 2) * zoom + camX
    local wy = (sy - h / 2) * zoom + camY
    return wx, wy
end


-- drawTextPanel(options)
-- Draws a rectangle with text inside. Can be left/right/center anchored,
-- support multi-line text, custom font, colors, and transparency.
function drawTextPanel(args)
    local text        = args.text
    local font        = args.font or getFont(14)
    local x           = args.x or 0
    local y           = args.y or 0
    local align       = args.align or "left"
    local anchor      = args.anchor or "topleft"
    local padding     = args.padding or 12
    local bgColor     = args.bgColor or {0.1, 0.1, 0.1, 0.9}
    local borderColor = args.borderColor or {1, 1, 1, 0.3}
    local defaultColor = args.textColor or {1, 1, 1, 1}
    local forceWidth = args.forceWidth or false

    love.graphics.setFont(font)

    -- Normalize lines
    local lines = {}
    if type(text) == "table" then
        for _, v in ipairs(text) do
            if type(v) == "string" then
                table.insert(lines, { text = v, color = defaultColor })
            elseif type(v) == "table" then
                table.insert(lines, {
                    text = v.text or "",
                    color = v.color or defaultColor,
                    isHeader = v.isHeader or false
                })
            end
        end
    else
        for s in tostring(text):gmatch("[^\n]+") do
            table.insert(lines, { text = s, color = defaultColor })
        end
    end

    -- Compute dimensions
    local lineHeight = font:getHeight() + 4
    local panelHeight = #lines * lineHeight + padding * 2
    if forceWidth then
        panelWidth = args.width
    else
        local maxWidth = 0
        for _, s in ipairs(lines) do
            local w = font:getWidth(s.text)
            if w > maxWidth then maxWidth = w end
        end
        panelWidth = maxWidth + padding * 2
    end

    -- Position by anchor
    local panelX, panelY = x, y
    if anchor == "topright" then
        panelX = x - panelWidth
    elseif anchor == "bottomleft" then
        panelY = y - panelHeight
    elseif anchor == "bottomright" then
        panelX = x - panelWidth
        panelY = y - panelHeight
    elseif anchor == "center" then
        panelX = x - panelWidth / 2
        panelY = y - panelHeight / 2
    end

    -- Background + border
    love.graphics.setColor(bgColor)
    love.graphics.rectangle("fill", panelX, panelY, panelWidth, panelHeight, 8, 8)

    love.graphics.setColor(borderColor)
    love.graphics.rectangle("line", panelX, panelY, panelWidth, panelHeight, 8, 8)

    -- Draw text with section headings highlighted
    local yCursor = panelY + padding
    for _, line in ipairs(lines) do
        if line.isHeader then
            -- colored translucent background for section header
            local c = line.color
            love.graphics.setColor(c[1], c[2], c[3], 0.25)
            love.graphics.rectangle("fill", panelX + 4, yCursor - 2, panelWidth - 8, lineHeight + 2, 4, 4)
            love.graphics.setColor(1, 1, 1, 0.9)
        else
            love.graphics.setColor(line.color)
        end

        local textX = panelX + padding
        if align == "center" then
            textX = panelX + (panelWidth - font:getWidth(line.text)) / 2
        elseif align == "right" then
            textX = panelX + panelWidth - font:getWidth(line.text) - padding
        end

        love.graphics.print(line.text, textX, yCursor)
        yCursor = yCursor + lineHeight
    end

    return panelWidth, panelHeight
end

-- Graham scan convex hull (simple, robust, 2D)
function convexHull(points)
    table.sort(points, function(a, b)
        return a[1] < b[1] or (a[1] == b[1] and a[2] < b[2])
    end)
    local cross = function(o, a, b)
        return (a[1] - o[1]) * (b[2] - o[2]) - (a[2] - o[2]) * (b[1] - o[1])
    end
    local lower = {}
    for _, p in ipairs(points) do
        while #lower >= 2 and cross(lower[#lower - 1], lower[#lower], p) <= 0 do
            table.remove(lower)
        end
        table.insert(lower, p)
    end
    local upper = {}
    for i = #points, 1, -1 do
        local p = points[i]
        while #upper >= 2 and cross(upper[#upper - 1], upper[#upper], p) <= 0 do
            table.remove(upper)
        end
        table.insert(upper, p)
    end
    table.remove(upper, 1)
    table.remove(upper, #upper)
    for _, p in ipairs(upper) do
        table.insert(lower, p)
    end
    return lower
end

function wrapTextToWidth(text, font, maxWidth)
    local lines = {}
    local line = ""
    for word in text:gmatch("%S+") do
        local try = (line == "" and word) or (line .. " " .. word)
        if font:getWidth(try) <= maxWidth then
            line = try
        else
            if line ~= "" then table.insert(lines, line) end
            line = word
        end
    end
    if line ~= "" then table.insert(lines, line) end
    return lines
end
