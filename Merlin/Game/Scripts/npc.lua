
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
    -- Bounds are given in cm
    local motherPos = mx.worldPos(mother)
    local npcSpatialBounds = mx.composeBounds(motherPos, mxu.float3(30,100,175), mxu.quat(0,0,0,1))
    local lhandSpatialBounds = mx.composeBounds(mxu.float3(0,-4,0), mxu.float3(10,10,10), mxu.quat(0,0,0,1))
    local rhandSpatialBounds = mx.composeBounds(mxu.float3(0,4,0), mxu.float3(10,10,10), mxu.quat(0,0,0,1))
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

function birthRootNpc(gender, socialClass, knowledge, rules, manSurnameSymbol)
    -- Root NPCs are born around the year 1720
    local birthYear = 1720
    local birthMonth = 3  -- April (0-based), matching sim start
    local birthDay = 0
    local birthTime = mx.makeSeconds(birthYear, birthMonth, birthDay, 6, 0, 0)
    -- Bounds are given in cm
    local npcSpatialBounds = mx.composeBounds(mxu.float3(0,0,0), mxu.float3(30,100,175), mxu.quat(0,0,0,1))
    local lhandSpatialBounds = mx.composeBounds(mxu.float3(0,-4,0), mxu.float3(10,10,10), mxu.quat(0,0,0,1))
    local rhandSpatialBounds = mx.composeBounds(mxu.float3(0,4,0), mxu.float3(10,10,10), mxu.quat(0,0,0,1))
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
    if gender == "female" then
        nameSymbol = generateRootNpcWifeNameSymbol(gender, socialClass, luaNationality, manSurnameSymbol)
    else
        nameSymbol = generateRootNpcNameSymbol(gender, socialClass, luaNationality)
    end
    mx.setSymbolAttr(npc, "name", nameSymbol)
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

function createRootNpcs()
    -- Upper class families (creating 2 couples = 4 NPCs)
    for i=0,1 do
        local man = birthRootNpc("male", "upper", {}, "Npc")
        local wife = birthRootNpc("female", "upper", {}, "Npc", mx.attrSymbol(man, "name"))
        -- Put them in a theatre - so they can show their kids it later
        local manObb = mx.worldBounds(mx.attrSymbol(man, "obb"))
        local wifeObb = mx.worldBounds(mx.attrSymbol(wife, "obb"))
        local manSize = mxu.float3(manObb.floats[3], manObb.floats[4], manObb.floats[5])
        local wifeSize = mxu.float3(wifeObb.floats[3], wifeObb.floats[4], wifeObb.floats[5])
        local unitQuat = mxu.quat(0,0,0,1)
        local theatre = theatres.buffer[0]
        local theatrePos = mx.worldPos(theatre)
        mx.setLocalBoundsAttr(man, "obb", mx.composeBounds(theatrePos, manSize, unitQuat))
        mx.setLocalBoundsAttr(wife, "obb", mx.composeBounds(theatrePos, wifeSize, unitQuat))
        mx.allPerceive()
        -- Pick a home for them - the first manor available
        local home = manors.buffer[0]
        mx.removeSymbolByIndex(manors, 0)
        -- Put them in their home
        local homePos = mx.worldPos(home)
        local wifePos = mxu.displace(homePos, 0, 300, 0)
        mx.setLocalBoundsAttr(man, "obb", mx.composeBounds(homePos, manSize, unitQuat))
        mx.setLocalBoundsAttr(wife, "obb", mx.composeBounds(wifePos, wifeSize, unitQuat))
        resolveNpcOverlaps(man, 4)
        -- Enable them to see each other & the environment
        mx.allPerceive()
        -- Make them believe they are married
        mx.enterMind(man)
        mx.believe("{@self spouse ?wife}", mxu.makeBindings({"?wife",mx.observe(wife)}))
        mx.enterMind(wife)
        mx.believe("{@self spouse ?man}", mxu.makeBindings({"?man",mx.observe(man)}))
        -- Make the man believe he owns his home
        local propertyNameSymbol = mx.attrSymbol(home, "name")
        local ownerNameSymbol = mx.attrSymbol(man, "name")
        local ownerKindSymbol = mx.hstrSymbol("[k human]")
        local ownershipDateSymbol =  mx.dateSymbol()
        local titleDeedWritings = composeTitleDeedWritings(propertyNameSymbol,ownerNameSymbol,ownerKindSymbol,ownershipDateSymbol)
        mx.enterMind(man)
        mx.readMsg(titleDeedWritings)
    end
    mx.enterAbsMind()
end
