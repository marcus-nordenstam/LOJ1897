
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
    -- 100m wide sectors (in meters)
    mx.makeWorldFromTerrain("Env/island.ter", 100)
    -- Load the scene layout (objects, buildings, spaces, set dressing etc)
    --mxu.loadSceneLayout("Env/layout.dat", kindToSystemTable)

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