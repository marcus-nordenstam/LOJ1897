
merlin = require "MerlinLuaDyn"


--  If you are a founder, and you are founding an org:
--      You know the orgName, cats, workplaceCats, headProfName, etc.
--      If the org doesn't have a workplace, then:

--          0. Create stacks etc
--          1. Create job descriptions
--          2. Create one employee record about yourself as the head of the org

--          0. Your org has a goal to achieve a building of workplaceCats among its properties
--          1. If you are NOT the Land Registry clerk, then tell the Land Registry clerk about your orgs goal 
--          2. This will result in the LR clerk creating a deed document, that they give you
--          3. You will then read this deed (because you read all docs given to you)
--              The info in the deed will tell you that the org owns the property, and you have a rule that says
--              that if the org has no workplace and the org owns a building, then the org's workplace is that building.
--          4. The information in step (3) causes this rule to terminate since the org now has a workplace. 



--      If the org has a workplace, then:
--          3. Create AoI doc now that we have a workplace
--          4. If you are NOT the Companies House clerk, then mail or give the AoI doc to that clerk
--          5. Once the AoI is in the companyRegistry stack, you end the founding org activity
--      
--  If you are working and have the admin task:
--      Create adverts for unfilled jobs ..etc


--  If you are land registry clerk:
--      If an org wishes to get a property, etc.

--  If you are a companies house clerk:
--      If you know of an AoI document that is NOT in the companies registry stack, then add it to the companies registry stack.
--      

-- Keep track of all founders
founders = {}

-- NOTE: 'org' is a mental object symbol from the mind of the GM, representing the org
function makeOrgFounder(org, isLandRegistryOrg)    
    -- For every org that should be founded that year, the GM must select a founder
    --      The founder must be at least 25 years old, male, and middle or upper-class.
    --      If a naturally born NPC can be found that matches this, pick them.
    --      If not, then create a parentless NPC for the founder.
    local founder = birthParentlessNpc("male", "middle", {}, "OriginalSettlerActions", "OriginalSettler")
    table.insert(founders, founder)
    -- Transfer all knowledge about this org from the GM directly to the founder
    local founderOrg = mx.learnAbout(founder, org)
    -- If the NPC is founding the land registry, then they need to know about all title deeds
    -- print("isLandRegistry:", isLandRegistryOrg)
    if isLandRegistryOrg ~= 0 then
        -- Create the deeds stack from all buildings
        local deedStack = makeStackEntity({1000, 1000, 1000})
        local dateSymbol = mx.dateSymbol()
        for _, layoutEntity in ipairs(layoutEntities) do
            if (mx.isA(layoutEntity, "physical building") or mx.isA(layoutEntity, "space")) then
                local addrSymbol = mx.attrSymbol(layoutEntity, "name")
                local propertyKindSymbol = mx.list(mx.attrSymbol(layoutEntity, "kind"))
                local writings = composeAvailableTitleDeedWritings(propertyKindSymbol, addrSymbol, dateSymbol)
                local deed = makeDoc("physical document titleDeed", writings)
                addItemToStack(deed, deedStack)
                -- Let the founder know about all buildings etc
                mx.read(founder, deed, "writings")
            end
        end
    end
    -- Give the founder org-founder behaviour
    mx.enterMind(founder)
    mx.import("FoundOrg")
    -- Make the NPC believe they are a founder of an org of the given name and categories
    -- This line will kick off all the behaviour that will (eventually) result in the org being founded.
    mx.event("{?org founder @self}", {["?org"]=founderOrg})
end

-- NOTE: All the arguments are mental symbols from the mind of the land-registry clerk
function updateTitleDeed(clerkSymbol, deedSymbol, propertyKindSymbol, addrSymbol, ownerCatsSymbol, ownerNameSymbol, dateSymbol)
    print("propertyCatsSymbol " .. mx.toLuaString(propertyKindSymbol))
    local clerkEnt = mx.entity(clerkSymbol)
    local deedEnt = mx.entity(deedSymbol)
    local writings = composeTitleDeedWritings(propertyKindSymbol, addrSymbol, ownerCatsSymbol, ownerNameSymbol, dateSymbol)
    mx.setSymbolAttr(deedEnt, "writings", writings)
    mx.read(clerkEnt, deedEnt, "writings")
end

-- LandRegistry founding:
--  Once a workplace is acquired, move all stacks into that workplace

-- Acquire a workplace
-- Creating your stacks (once a workplace is had - stacks go in the workplace)
-- 

function broadcastOrgInfoToFounders(essential)
    -- If the org being founded is an essential org, then add info about it to the essentialOrgs document
    if essential then
        local orgInfoWriting = composeOrgInfoWriting(mx.hstrSymbol(orgName), mx.hstrSymbol(orgCats), mx.hstrSymbol(orgAddr))
        mx.addSymbolAttr(essentialOrgsDoc, "writings", orgInfoWriting)
        -- Ensure that all founders know about all essential orgs, even when orgs are founded after the founder was made
        for _, curFounder in ipairs(founders) do
            mx.read(curFounder, essentialOrgsDoc, "writings")
        end
    else    
        -- Every NEW founder must read the current founder-knowledge doc
        mx.read(founder, essentialOrgsDoc, "writings")
    end
end


function startOrg(orgName, workplaceCategories, headProfName, workerProfNames)
    local workplace = acquireWorkplace(orgName, workplaceCategories)
    local workplaceObb = mx.attrSymbol(workplace, "obb")
    local workplacePos = workplaceObb[1]

    local mail = makeStackEntity(workplacePos)
    local outMail = makeStackEntity(workplacePos)
    local apps = makeStackEntity(workplacePos)

    --NOTE: composeArticles and composeJobDescrption etc - NEED TO SUPPLY MERLIN SYMBOLS for their args

    local aoiWritings = composeArticlesOfIncorpWritings(orgName, orgCats, workplaceAddr, date)
    local aoi = makeDoc("physical document articlesOfIncorporation", aoiWritings)    

    --for the headProf and every workerProf:
    local jobDescriptionStack = makeStackEntity({1000,1000,1000})
    local jobDescription = composeJobDescription()
    local jobDescriptionDoc = makeDoc("physical document jobDescription", jobDescription)
    addDocToStack(jobDescriptionDoc, jobDescriptionStack)
    -- There are no employees yet, but we make the stack for them
    local employeeRecords = makeStackEntity(workplacePos)
    return aoi
end
