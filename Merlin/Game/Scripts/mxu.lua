
-- Regular utils
function contains(list, item)
    for _, value in ipairs(list) do
        if value==item then
            return true
        end
    end
    return false
end

-- Some utilities & helpers that also handle marshalling between Lua/FFI/C memory

mxu = mxu or {}


function mxu.setRandomSeed(seed)
    math.randomseed(seed)
    mx.setRandomSeed(seed)
end

function mxu.iter(mxarray)
    local i = -1
    return function()
        i = i + 1
        if i < mxarray.size then
            return i+1, mxarray.buffer[i]
        end
    end
end

function mxu.actIter(mxarray)
    local i = -1
    return function()
        i = i + 1
        if i < mxarray.size then
            return i+1, mxarray.acts[i], mxarray.util[i], mxarray.outcompetedBy[i]
        end
    end
end

function mxu.sectorIter(mxarray)
    local i = -1
    return function()
        i = i + 1
        if i < mxarray.size then
            return i+1, mxarray.minCoords[i], mxarray.maxCoords[i]
        end
    end
end

function mxu.initMerlinSymbols()
    msym = {}
    msym.fail = mx.hstrSymbol("@fail")
    msym.human = mx.hstrSymbol("human")
    msym.pregnantWhen = mx.hstrSymbol("pregnantWhen")
    msym.nothing = mx.hstrSymbol("@nothing")
    msym.sometime = mx.hstrSymbol("!@nothing")
    msym.t = mx.hstrSymbol("@true")
    msym.f = mx.hstrSymbol("@false")
    msym.male = mx.hstrSymbol("male")
    msym.female = mx.hstrSymbol("female")
    msym.sister = mx.hstrSymbol("sister")
    msym.brother = mx.hstrSymbol("brother")
    msym.alert = mx.hstrSymbol("alert");
    msym.sleepy = mx.hstrSymbol("sleepy");
    msym.conversation = mx.hstrSymbol("conversation")
    msym.participant = mx.hstrSymbol("participant")
end

function mxu.toLuaString(symbol)
    return ffi.string(mx.toString(symbol).buffer)
end

function mxu.toNlLuaString(symbol)
    return ffi.string(mx.toNlString(symbol).buffer)
end

function mxu.displace(pos, dx, dy, dz)
    local dpos = pos
    dpos.floats[0] = dpos.floats[0] + dx
    dpos.floats[1] = dpos.floats[1] + dy
    dpos.floats[2] = dpos.floats[2] + dz
    return dpos
end

function mxu.scale(pos, sx, sy, sz)
    local dpos = pos
    dpos.floats[0] = dpos.floats[0] * sx
    dpos.floats[1] = dpos.floats[1] * sy
    dpos.floats[2] = dpos.floats[2] * sz
    return dpos
end

function mxu.isValidBounds(bounds)
    return bounds.floats[3] ~= 0 and
           bounds.floats[4] ~= 0 and
           bounds.floats[5] ~= 0
end
function mxu.isNullBounds(bounds)
    return bounds.floats[3] == 0 and
           bounds.floats[4] == 0 and
           bounds.floats[5] == 0
end

function mxu.containsStr(array, luaStr)
    for i = 0, array.size - 1 do
        local luaItemStr = mxu.toLuaString(array.buffer[i])
        if luaItemStr == luaStr then
            return true
        end
    end
end

function mxu.containsSymbol(array, symbol)
    for i = 0, array.size - 1 do
        if array.buffer[i] == symbol then
            return true
        end
    end
end

function mxu.float3(x, y, z)
    local float3 = ffi.new("MerlinFloat3")
    float3.floats[0] = x
    float3.floats[1] = y
    float3.floats[2] = z
    return float3
end

function mxu.quat(x, y, z, w)
    local quat = ffi.new("MerlinQuat")
    quat.floats[0] = x
    quat.floats[1] = y
    quat.floats[2] = z
    quat.floats[3] = w
    return quat
end

function mxu.obbPos(obb)
    return mxu.float3(obb.floats[0], obb.floats[1], obb.floats[2])
end

function mxu.obbRadius(obb)
    return math.max(obb.floats[3], obb.floats[4], obb.floats[5])
end

function mxu.setObbPos(obb, x, y, z)
    obb.floats[0] = x
    obb.floats[1] = y
    obb.floats[2] = z
end

function mxu.setObbQuat(obb, x, y, z, w)
    obb.floats[6] = x
    obb.floats[7] = y
    obb.floats[8] = z
    obb.floats[9] = w
end

function mxu.list(list)
    local out = ffi.new("MerlinSymbolArray")
    for i = 1, #list do
        local symbol
        if type(list[i]) == "string" then
            symbol = mx.hstrSymbol(list[i])
        else
            symbol = list[i]
        end
        mx.addSymbol(out, symbol)
    end
    return mx.list(out)
end

function mxu.makeBindings(tbl)
    local bindings = ffi.new("MerlinVarBindings[1]")
    local bindingsRef = bindings[0]

    local n = #tbl
    assert(n % 2 == 0, "table must contain pairs: {name, value, name, value, ...}")

    local numBindings = math.floor(n / 2)
    bindingsRef.numBindings = numBindings

    for i = 0, numBindings - 1 do
        local varName = tbl[2*i + 1]
        local value   = tbl[2*i + 2]
        -- assign string: LuaJIT auto-converts Lua string to const char*
        bindingsRef.varNames[i] = varName
        -- assign value: ensure it's 64-bit
        bindingsRef.values[i] = value
    end

    return bindings
end

function mxu.loadSceneLayout(sceneLayoutFilename, kindToSystemTable)
    -- Convert lua table to FFI table:
    -- count pairs
    local count = 0
    for _ in pairs(kindToSystemTable) do count = count + 1 end
    -- allocate array: 2 entries per pair (key,value)
    local flat = ffi.new("const char*[?]", count * 2)
    local i = 0
    for k, v in pairs(kindToSystemTable) do
        flat[i]   = ffi.new("char[?]", #k+1, k) -- C string copy of key
        flat[i+1] = ffi.new("char[?]", #v+1, v) -- C string copy of value
        i = i + 2
    end
    mx.loadSceneLayout(sceneLayoutFilename, flat, count)
end

function mxu.printTable(tbl, indent)
    indent = indent or 0
    local formatting = string.rep("  ", indent)

    if type(tbl) ~= "table" then
        print(formatting .. tostring(tbl))
        return
    end

    print(formatting .. "{")
    for key, value in pairs(tbl) do
        local keyStr = type(key) == "string" and string.format('["%s"]', key) or string.format("[%s]", key)
        if type(value) == "table" then
            print(formatting .. "  " .. keyStr .. " = ")
            mxu.printTable(value, indent + 1)
        else
            print(formatting .. "  " .. keyStr .. " = " .. tostring(value) .. ",")
        end
    end
    print(formatting .. "}")
end

mxu.noBind = mxu.makeBindings({})

-- Callbacks

_callbacks   = _callbacks   or {}  -- keep ffi.cast objects alive
_luaHandlers = _luaHandlers or {}  -- Lua handlers keyed by name

-- single FFI callback trampoline
local dispatcher = ffi.cast("MerlinCallback", function(arg1, arg2, arg3, arg4)
    -- we’ll always be called with a uint64_t argument
    -- the first 32 bits can be used as a hash of the funcName,
    -- or simpler: we just store one dispatcher per funcName below
    return ffi.new("uint64_t", 0) -- placeholder, will be overridden
end)

function mxu.safeRegisterCallback(name, lua_fn)
    -- make a wrapped ffi callback that catches errors
    local cb = ffi.cast("MerlinCallback", function(arg1, arg2, arg3, arg4)
        local ok, res = pcall(lua_fn, arg1, arg2, arg3, arg4)
        if not ok then
            io.stderr:write("callback '", name, "' error: ", tostring(res), "\n")
            return ffi.new("uint64_t", 0)
        end
        if type(res) == "number" then
            return ffi.new("uint64_t", res)
        elseif ffi.istype("uint64_t", res) then
            return res
        else
            return ffi.new("uint64_t", 0)
        end
    end)

    -- keep both Lua fn and ffi callback alive
    _luaHandlers[name] = lua_fn
    _callbacks[name]   = cb

    -- hand pointer to C
    mx.registerCallback(name, cb)
end

function mxu.safeRegisterSoundCallback(lua_fn)
    -- make a wrapped ffi callback that catches errors
    local cb = ffi.cast("SoundCallback", function(arg)
        local ok = pcall(lua_fn, arg)
        if not ok then
            io.stderr:write("Merlin Environment Sound callback error: ", tostring(res), "\n")
            return
        end
    end)
    -- keep both Lua fn and ffi callback alive
    _luaHandlers["SoundCallback"] = lua_fn
    _callbacks["SoundCallback"]   = cb
    -- hand pointer to C
    mx.registerSoundCallback(cb)
end


-- RequestWorkplace(org, category):
--  Enter the mind of the Land Registry clerk.  The NPC-entity is accessed via a global variable.
--  Search for a property that matches category and whose owner is @nothing
--  Update deed to state that org now owns property
--  Have the clerk read the updated deed
--  return building-entity


-- The NULL Mind keeps track of which entity is the current Land Registry clerk.

-- Land Registry can handle two types of requests/actions:
--  RequestWorkplace() - only used by orgs to get their workplace.
--      The clerk finds an available workplace with the right category and
--      assigns its owner to the org.  It returns the workplace.
--  The other action is that the seller of a property sends a letter of conveyance
--  to the Land Registry, which then processes the transfer.  This is how private
--  property transactions are handled, e.g. when a family buys their home.

--  Then, create Jersey Land Registry org:
--      Create a "deedsRegistry" stack, adding all deeds to it
--      Create the head NPC which is also the clerk
--          Set the Land Registry Clerk NPC-entity as a global variable
--      Have them read all the deeds so that they can perform RequestWorkplace:
--
--      ** The following is general for any org that is started:
--      workplace = RequestWorkplace(Jersey Land Registry, bldgCat)
--      Create the Articles of Incorporation for the Jersey Land Registry, specifying:
--          The selected property as its workplace.
--          The NPC as the head
--      Create two additional stacks "employeeRecords" and "jobDescriptions"
--      Also create Employee Record docs for each employee:
--          Stating the name and what job they hold
--      Add all records to "employeeRecords" stack
--      Also create Job Descriptions for all jobs:
--          Stating the name or category of job
--          Who has the job, or @nothing if nobody has it
--      Add all descriptions to "jobDescriptions" stack
--      Return the articles of incorporation document

--  Then, create Jersey Companies House org:
--      Create the head NPC/clerk for Jersey Companies House
--      workplace = RequestWorkplace(Jersey Companies House, bldgCat)
--      Create the Articles of Incorporation for the Jersey Companies house:
--          Giving both workplace and NPC as head / clerk job
--      Create a "companyRegistry" stack
--      Add the Articles of Incorporation to the "companyRegistry" stack
--      Add the (given) AoI for Jersey Land Registry to the stack as well
--      Have the head/clerk read all the articles of incorporation


--  Create articles of incorporation document listing general info about the org
--  Create employee records for head & worker positions (unfilled)
--  Birth parentless NPCs for each unfilled record
--  Update employee records to reflect each NPC taking on each job
--  Have each NPC read their employee record
--  Have the head NPC / founder read ALL employee records
--  Register the articles of incorporation at the Jersey Companies


