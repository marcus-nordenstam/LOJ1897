
function generateRootNpcNameSymbol(gender, socialClass, nationality)
    -- compose the name kind strings to match the convention used in the Ontology
    -- e.g. "femaleEnglishName", "upperClassJerseySurname", etc
    local firstNameStr = gender .. nationality .. "Name"
    local surnameStr
    if (socialClass == "upper") then
        surnameStr = "upperClass" .. nationality .. "Surname"
    else
        surnameStr = "common" .. nationality .. "Surname"
    end
    return mx.assembleRandomFullName("human_npc", firstNameStr, surnameStr)
end

function generateRootNpcWifeNameSymbol(gender, socialClass, nationality, manNameSymbol)
    -- compose the name kind strings to match the convention used in the Ontology
    -- e.g. "femaleEnglishName", "upperClassJerseySurname", etc
    local firstNameStr = gender .. nationality .. "Name"
    return mx.assembleRandomName("human_npc", firstNameStr, manNameSymbol)
end

function generateChildNpcNameSymbol(gender, nationality, father)
    -- compose the name kind strings to match the convention used in the Ontology
    -- e.g. "femaleEnglishName", "upperClassJerseySurname", etc
    local firstNameStr = gender .. nationality .. "Name"
    local surname = mx.attrSymbol(father, "name")
    return mx.assembleRandomName("human_npc", firstNameStr, surname)
end

function loadAttrTable(tablesDir)
    -- List all .txt files
    local tableFiles = mx.ls(tablesDir, "txt")
    -- If there is a count.txt, make a table of it
    local countsTable = ffi.new("CountsTable[1]")
    if(mxu.containsStr(tableFiles, "counts.txt")) then
        mx.makeCountsTable(countsTable, tablesDir .. "/counts.txt")
    end
    local attrTable = ffi.new("AttrTable[1]", {NULL,NULL,NULL,0,0})
    for i = 0, tableFiles.size - 1 do
        local filename = mxu.toLuaString(tableFiles.buffer[i])
        local luaFilename = ffi.string(filename) -- convert from c-string to lua-string
        local attrName = string.gsub(luaFilename, "[.]%w%w%w$", "")
        if(attrName ~= "counts") then
            local valueTable = ffi.new("ValueTable[1]", {NULL,NULL,0})
            mx.makeValueTable(valueTable, tablesDir .. "/" .. luaFilename)
            mx.updateAttrTable(attrTable, attrName, valueTable, countsTable)
        end
    end
    return attrTable
end

function isPluralTrait(npcTable, attrName)
    return mx.numAttrTableValues(npcTable, attrName) > 1
end

-- Returns a MerlinSymbolArray or raw symbol (ffi.uint64_t)
function sampleAttrTable(npcTable, attrName)
    local numValues = mx.numAttrTableValues(npcTable, attrName)
    if (numValues == 1) then
        -- Deal with singular attributes
        return mx.hstrSymbol(mx.sampleAttrTable(npcTable, attrName))
    else
        -- Deal with list attributes
        local pluralValue = ffi.new("MerlinSymbolArray[1]")
        local pluralValueRef = pluralValue[0]
        while pluralValueRef.size < numValues do
            local value = mx.hstrSymbol(mx.sampleAttrTable(npcTable, attrName))
            while mxu.containsSymbol(pluralValueRef, value) do
                value = mx.hstrSymbol(mx.sampleAttrTable(npcTable, attrName))
            end
            mx.addSymbol(pluralValue, value)
        end
        return pluralValue
    end
end

function sampleInheritedTrait(traitName, mother, father, randomTrait)
    local motherTrait = mx.attrSymbol(mother, traitName)
    local fatherTrait = mx.attrSymbol(father, traitName)
    local traitSamples = {randomTrait, motherTrait, fatherTrait}
    return traitSamples[math.random(#traitSamples)]
end

function sampleChildNpcTrait(npcTable, traitName, mother, father)
    local numValues = mx.numAttrTableValues(npcTable, traitName)
    if (numValues == 1) then
        -- Deal with singular attributes
        local randomTrait = mx.hstrSymbol(mx.sampleAttrTable(npcTable, traitName))
        return sampleInheritedTrait(traitName, mother, father, randomTrait)
    else
        -- Deal with list attributes
        local motherTraits = mx.attrSymbolArray(mother, traitName)
        local fatherTraits = mx.attrSymbolArray(father, traitName)
        local motherTrait = motherTraits.buffer[math.random(motherTraits.size)-1]
        local fatherTrait = fatherTraits.buffer[math.random(fatherTraits.size)-1]
        local pluralValue = ffi.new("MerlinSymbolArray[1]")
        local pluralValueRef = pluralValue[0]
        if (motherTrait ~= fatherTrait) then
            mx.addSymbol(pluralValueRef, motherTrait)
            mx.addSymbol(pluralValueRef, fatherTrait)
        else
            mx.addSymbol(pluralValueRef, motherTrait)
        end
        while pluralValueRef.size < numValues do
            local value = mx.hstrSymbol(mx.sampleAttrTable(npcTable, traitName))
            while mxu.containsSymbol(pluralValueRef, value) do
                value = mx.hstrSymbol(mx.sampleAttrTable(npcTable, traitName))
            end
            mx.addSymbol(pluralValueRef, value)
        end
        return pluralValue
    end
end

function createFinger(handSymbol, fingerKind, birthTime, fingerBounds)
    local fingerSymbol = mx.makeSubEntityAtTime("finger", fingerKind, birthTime, fingerBounds, handSymbol)
    mx.setOccluder(fingerSymbol, false)
    mx.setSymbolAttr(handSymbol, fingerKind, fingerSymbol)
    return fingerSymbol
end

function createHand(npcSymbol, handKind, birthTime, handBounds)
    local handSymbol = mx.makeSubEntityAtTime("hand", handKind, birthTime, handBounds, npcSymbol)
    mx.setOccluder(handSymbol, false)
    mx.setSymbolAttr(npcSymbol, handKind, handSymbol)
    local fingerBounds = mx.composeBounds(mxu.displace(mxu.scale(mxu.obbPos(handBounds), 1, 0.4, 1), 1, 0, 0), mxu.float3(5,1,1), mxu.quat(0,0,0,1))
    createFinger(handSymbol, "ringFinger", birthTime, fingerBounds)
    return handSymbol
end

function giveBirth(mother)
    -- Bounds are given in meters
    local motherPos = mx.worldPos(mother)
    local npcSpatialBounds = mx.composeBounds(motherPos, mxu.float3(0.3,1.0,1.75), mxu.quat(0,0,0,1))
    local lhandSpatialBounds = mx.composeBounds(mxu.float3(0,-0.04,0), mxu.float3(0.1,0.1,0.1), mxu.quat(0,0,0,1))
    local rhandSpatialBounds = mx.composeBounds(mxu.float3(0,0.04,0), mxu.float3(0.1,0.1,0.1), mxu.quat(0,0,0,1))
    local child = mx.makeRootEntityNow("human_npc", "human", npcSpatialBounds)
    mx.setOccluder(child, false)
    local lhand = createHand(child, "leftHand", mx.seconds(), lhandSpatialBounds)
    local rhand = createHand(child, "rightHand", mx.seconds(), rhandSpatialBounds)
    resolveNpcOverlaps(child, 4)
    -- Gender is random
    local gender = sampleAttrTable(npcNonTraitTable, "gender")
    mx.setSymbolAttr(child, "gender", gender)
    -- Traits are inherited from parents
    local father = mx.attrSymbol(mother, "pregnantBy")
    local npcTraitTableRef = npcTraitTable[0]
    for i = 0, npcTraitTableRef.size - 1 do
        local traitName = ffi.string(npcTraitTableRef.attrNames[i])
        local newAttrValue = sampleChildNpcTrait(npcTraitTable, traitName, mother, father)
        if isPluralTrait(npcTraitTable, traitName) then
            mx.setSymbolArrayAttr(child, traitName, newAttrValue)
        else
            mx.setSymbolAttr(child, traitName, newAttrValue)
        end
    end
    local extroversion = sampleInheritedTrait("extroversion", mother, father, mx.floatSymbol(math.random()))
    local intelligence = sampleInheritedTrait("intelligence", mother, father, mx.floatSymbol(math.random()))
    mx.setSymbolAttr(child, "extroversion", extroversion)
    mx.setSymbolAttr(child, "intelligence", intelligence)
    -- Nationality is wherever they are born, so Jersey
    local nationality = mx.hstrSymbol("Jersey")
    mx.setSymbolAttr(child, "nationality", nationality)
    -- Choose a unique child name
    local nameSymbol = generateChildNpcNameSymbol(mxu.toLuaString(gender), mxu.toLuaString(nationality), father)
    mx.setSymbolAttr(child, "name", nameSymbol)
    -- The child knows it's playing an NPC (and not a root-NPC)
    mx.enterMind(child)
    mx.believe("{@self role npc}", mxu.noBind)
    -- The child knows its parents
    local motherName = mx.attrSymbol(mother, "name")
    local fatherName = mx.attrSymbol(father, "name")
    local mmother = mx.observe(mother)
    local mfather = mx.observe(father)
    local bindings = mxu.makeBindings({ "?mother",      mmother,
                                        "?father",      mfather,
                                        "?motherName",  motherName,
                                        "?fatherName",  fatherName})
    mx.believe("{@self mother ?mother}", bindings)
    mx.believe("{@self father ?father}", bindings)
    mx.believe("{?mother name ?motherName}", bindings)
    mx.believe("{?father name ?fatherName}", bindings)
    mx.believe("{@self parent ?parent}", mxu.makeBindings({"?parent",mmother}))
    mx.believe("{@self parent ?parent}", mxu.makeBindings({"?parent",mfather}))
        -- The child uses Npc rules (as opposed to RootNpc rules)
    mx.import("Npc")

    -- The parents know about their child
    mx.enterMind(mother)
    local mchild = mx.observe(child)
    mx.believe("{@self child ?child}", mxu.makeBindings({"?child",mchild}))
    mx.believe("{?child name ?name}", mxu.makeBindings({"?name",nameSymbol,"?child",mchild}))
    mx.enterMind(father)
    mchild = mx.observe(child)
    mx.believe("{@self child ?child}", mxu.makeBindings({"?child",mchild}))
    mx.believe("{?child name ?name}", mxu.makeBindings({"?name",nameSymbol,"?child",mchild}))
    mx.enterAbsMind()
    -- The child NPC is now ready for simulation
    return child
end

function siblingRel(gender)
    if(gender == msym.female) then
        return msym.sister
    else
        return msym.brother
    end
end

function birthRootNpc(gender, socialClass, knowledge, rules, manSurnameSymbol, spawnPos, modelPath)
    -- Root NPCs are born around the year 1720
    local birthYear = 1720
    local birthMonth = 3  -- April (0-based), matching sim start
    local birthDay = 0
    local birthTime = mx.makeSeconds(birthYear, birthMonth, birthDay, 6, 0, 0)
    -- Bounds are given in meters
    -- spawnPos is optional: if provided, use it; otherwise use (0,0,0)
    local pos = spawnPos or mxu.float3(0,0,0)
    -- modelPath is optional: path to GRYM model file for rendering
    modelPath = modelPath or ""
    local npcSpatialBounds = mx.composeBounds(pos, mxu.float3(0.3,1.0,1.75), mxu.quat(0,0,0,1))
    local lhandSpatialBounds = mx.composeBounds(mxu.float3(0,-0.04,0), mxu.float3(0.1,0.1,0.1), mxu.quat(0,0,0,1))
    local rhandSpatialBounds = mx.composeBounds(mxu.float3(0,0.04,0), mxu.float3(0.1,0.1,0.1), mxu.quat(0,0,0,1))
    local npc = mx.makeRootEntityAtTime("human_npc", "human", birthTime, npcSpatialBounds)
    mx.setOccluder(npc, false)

    --mx.startTrace(npc)
    --mx.traceEvent("{? ageGroup ? ?}")

    local lhand = createHand(npc, "leftHand", birthTime, lhandSpatialBounds)
    local rhand = createHand(npc, "rightHand", birthTime, rhandSpatialBounds)
    -- For parentless NPCs, gender is given, so don't randomize it
    mx.setSymbolAttr(npc, "gender", mx.hstrSymbol(gender))
    -- Nationality is random
    local nationality = sampleAttrTable(npcNonTraitTable, "nationality")
    mx.setSymbolAttr(npc, "nationality", nationality)
    -- Select random values for all traits
    local npcTraitTableRef = npcTraitTable[0]
    for i = 0, npcTraitTableRef.size - 1 do
        local traitName = ffi.string(npcTraitTableRef.attrNames[i])
        local newAttrValue = sampleAttrTable(npcTraitTable, traitName)
        if isPluralTrait(npcTraitTable, traitName) then
            mx.setSymbolArrayAttr(npc, traitName, newAttrValue)
        else
            mx.setSymbolAttr(npc, traitName, newAttrValue)
        end
    end
    -- Deal with traits that don't appear in the table
    mx.setSymbolAttr(npc, "extroversion", mx.floatSymbol(math.random()))
    mx.setSymbolAttr(npc, "intelligence", mx.floatSymbol(math.random()))
    -- Pick a name, once we know the nationality (set in the attr-loop above)
    local nameSymbol
    local luaNationality = mxu.toLuaString(nationality)
    if gender == "female" and manSurnameSymbol ~= nil then
        nameSymbol = generateRootNpcWifeNameSymbol(gender, socialClass, luaNationality, manSurnameSymbol)
    else
        nameSymbol = generateRootNpcNameSymbol(gender, socialClass, luaNationality)
    end
    mx.setSymbolAttr(npc, "name", nameSymbol)
    -- Store GRYM model path for rendering
    if modelPath ~= "" then
        mx.setSymbolAttr(npc, "modelPath", mx.hstrSymbol(modelPath))
    end
    -- Learn initial knowledge by reading any given documents
    --for _, doc in ipairs(knowledge) do
    --    mx.read(npc, doc, "writings")
    --end
    -- Assign NPC role & behaviour
    mx.enterMind(npc)
    mx.believe("{@self role rootNpc}", mxu.makeBindings({}))
    if rules ~= "" then
        mx.import(rules)
    end
    mx.enterAbsMind()
    return npc
end

function injectWaypointKnowledge(npc, waypointEntity, waypointObb)
    -- Inject knowledge about a waypoint into an NPC's mind
    -- Follows the pattern from Merlin/Game/Knowledge/Waypoint.mc
    mx.enterMind(npc)
    
    -- Create mental representation of the waypoint
    local mwaypoint = mx.observe(waypointEntity)
    
    -- Create bindings for ?waypoint and ?obb variables
    local bindings = mxu.makeBindings({
        "?waypoint", mwaypoint,
        "?obb", waypointObb
    })
    
    -- Inject each pattern from Waypoint.mc
    mx.believe("{ ?waypoint isa [k waypoint] }", bindings)
    mx.believe("{ ?waypoint obb ?obb }", bindings)
    
    mx.enterAbsMind()
end

function createRootNpcs(spawnPoints, npcModelPath, waypointPositions)
    -- Create NPCs at spawn points from scene metadata
    -- spawnPoints: table of {x, y, z} positions in meters
    -- npcModelPath: path to NPC model file for GRYM rendering
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
    
    -- Create waypoint entities in Merlin environment
    local waypointEntities = {}
    local birthTime = mx.makeSeconds(1720, 3, 0, 6, 0, 0)
    for i = 1, #waypointPositions do
        local wpPos = mxu.float3(waypointPositions[i].x, waypointPositions[i].y, waypointPositions[i].z)
        local wpSize = mxu.float3(1.0, 1.0, 1.0)  -- 1m cube
        local wpQuat = mxu.quat(0, 0, 0, 1)
        local wpObb = mx.composeBounds(wpPos, wpSize, wpQuat)
        
        -- Create waypoint entity (kind "waypoint" is set automatically by makeRootEntityAtTime)
        local waypoint = mx.makeRootEntityAtTime("waypoint", "waypoint", birthTime, wpObb)
        mx.setOccluder(waypoint, false)
        
        table.insert(waypointEntities, {entity = waypoint, obb = wpObb})
    end
    
    -- Loop through each spawn point and create a Merlin NPC
    for i = 1, #spawnPoints do
        local spawnPoint = spawnPoints[i]
        local spawnPos = mxu.float3(spawnPoint.x, spawnPoint.y, spawnPoint.z)
        
        -- Create a single NPC at this spawn point
        -- Randomize gender and social class for variety
        local gender = (math.random() < 0.5) and "male" or "female"
        local socialClass = (math.random() < 0.5) and "upper" or "common"
        local npc = birthRootNpc(gender, socialClass, {}, "Npc", nil, spawnPos, npcModelPath)
        
        -- Get NPC size for positioning
        local npcObb = mx.worldBounds(mx.attrSymbol(npc, "obb"))
        local npcSize = mxu.float3(npcObb.floats[3], npcObb.floats[4], npcObb.floats[5])
        local unitQuat = mxu.quat(0,0,0,1)
        
        -- Position NPC at spawn point
        mx.setLocalBoundsAttr(npc, "obb", mx.composeBounds(spawnPos, npcSize, unitQuat))
        resolveNpcOverlaps(npc, 4)
        
        -- Inject waypoint knowledge into this NPC's mind
        for j = 1, #waypointEntities do
            local wp = waypointEntities[j]
            injectWaypointKnowledge(npc, wp.entity, wp.obb)
        end
        
        -- Enable NPC to see the environment
        mx.allPerceive()
    end
    
    mx.enterAbsMind()
end
