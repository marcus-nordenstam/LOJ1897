
-- Make an empty document with no writing

function makeEmptyDoc(docKind)
    return merlin.makeEntityNow("prop", docKind, {{0,0,0},{1,1,1},{1,0,0,0}})
end

-- Make a document containing the given writings
-- Documents are structured as follows:
--
-- doc isa       kind
-- doc writings  [[sectionKind sentences] [sectionKind sentences] ...]
--
-- writings is a list of sections
-- each section is itself a list: [sectionKind sentences] 
-- sectionKind is a kind-symbol telling us the kind of section (chapter, declaration, etc)
-- all remaining symbols in the list are expected to be the written sentences.
--
-- So a title deed stating that John Smith owns 14 Victoria Ave, would be encoded as follows:
--
-- {{deed} isa      [k object document titleDeed]}
-- {{deed} writings [[k declaration] {[n 14 Victoria Ave] owner [n John Smith] /i ?date @ongoing}]}

function makeDoc(docKind, writingsSymbol)
    local doc = mx.makeEntityNow("prop", docKind, {{0,0,0},{1,1,1},{1,0,0,0}})
    mx.setSymbolAttr(doc, "writings", writingsSymbol)
    return doc
end

function addToStack(docInstEntity, stackEntity)
    mx.setSymbolAttr(docInstEntity, "inStack", stackEntity)
    mx.addSymbolAttr(stackEntity, "items", docInstEntity)
    mx.setSymbolAttr(stackEntity, "top", docInstEntity)
end


-- Compose writings

function composeObjectWritings(type, kindSymbol, obbSymbol)
    local bindings = {["?kind"]=kindSymbol,
                      ["?obb"]=obbSymbol}
    mx.enterAbsMind()
    local sentences = mx.learn("(o ?kind {@o obb ?obb})", bindings)
    return mx.list({type, sentences})
end

function composePropertyForSaleWritings(propertyNameSymbol, propertyKindSymbol, propertyObbSymbol, priceSymbol)
    local bindings = {["?name"]=propertyNameSymbol,
                      ["?kind"]=propertyKindSymbol,
                      ["?obb"]=propertyObbSymbol,
                      ["?price"]=priceSymbol}
    mx.enterAbsMind()
    local sentences = mx.learn("(msg {(o ?name ?kind {@o obb ?obb} {@o price ?price}) availability forSale})", bindings)
    return mx.list({"[k propertyDescription]", sentences})
end

function composeEssentialOrgInfoWritings(orgKind, orgName, workplaceEntity)
    local bindings = {["?orgKind"]=mx.hstrSymbol(orgKind),
                      ["?orgName"]=mx.hstrSymbol(orgName),
                      ["?workPlaceKind"]=mx.attrSymbol(workplaceEntity, "isa"),
                      ["?workPlaceName"]=mx.attrSymbol(workplaceEntity, "name"),
                      ["?obb"]=mx.attrSymbol(workplaceEntity, "obb")}
    mx.enterAbsMind()
    local sentences = mx.learn("(msg {(o ?orgKind ?orgName) workplace (o ?workPlaceKind ?workPlaceName {@o obb ?obb})})", bindings)
    return mx.list({"[k declaration]", sentences})
end




function composeTitleDeedWritings(propertyNameSymbol, ownerNameSymbol, ownerKindSymbol, ownershipDateSymbol)
    mx.enterAbsMind()
    local bindings = mxu.makeBindings({"?propertyName",propertyNameSymbol,
                                          "?ownerName",ownerNameSymbol,
                                          "?ownerKind",ownerKindSymbol,
                                          "?ownershipDate",ownershipDateSymbol})
    local ownershipStmt =
        mx.learn("(msg {(o ?propertyName) owner (o ?ownerName ?ownerKind) /i ?ownershipDate @ongoing})", bindings)
    return mxu.list({"[k titleDeed] ", ownershipStmt})
end





function composeArticlesOfIncorpWritings(orgNameSymbol, orgCatsSymbol, workplaceAddrSymbol)
    mx.enterAbsMind()
    local bindings = {["?orgName"]=orgNameSymbol, 
                      ["?orgCats"]=orgCatsSymbol, 
                      ["?address"]=workplaceAddrSymbol}
    local sentences = mx.event("(msg {obj() [name ?orgName ; categories /inc ?orgCats ; address ?address]})", bindings)
    return mx.list({"articlesOfIncorporation", sentences})
end

function composeJobDescription(profNameSymbol, orgNameSymbol, annualWageSymbol, levelSymbol)
    mx.enterAbsMind()
    local bindings = {["?profName"]=profNameSymbol, 
                      ["?orgName"]=orgNameSymbol, 
                      ["?annualWage"]=annualWageSymbol, 
                      ["?level"]=levelSymbol}
    local sentences = mx.event("{/msg obj() [categories /inc job ; profName ?profName ; org ?orgName ; annualWage ?annualWage ; level ?level ; holder @nothing]}", bindings)
    return mx.list({"jobDescription", sentences})
end

function createWorkerJobDescriptions(orgNameSymbol, workerProfNames, multiples)
    -- If we are adding multiple jobs / profession, 
    -- then set the levels according to this schedule:
    local numLevels = 4;
    local levelLut = {"regular", "junior", "senior", "regular"}
    for i=1,multiples do 
        local levelSymbol = mx.hstrSymbol(levelLut[i % numLevels])
        for _, workerProfName in ipairs(workerProfNames) do
            composeJobDescription(mx.hstrSymbol(workerProfName),
                                  orgNameSymbol, 
                                  mx.hstrSymbol(annualWage),
                                  levelSymbol)
        end
    end
end

