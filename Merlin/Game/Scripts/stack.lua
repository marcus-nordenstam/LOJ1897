
merlin = require "MerlinLuaDyn"

function makeStackEntity(pos)
    local dim = {50, 50, 50}
    local rot = mx.makeAngleAxisRot(0, {1,0,0})
    local obb = {pos, dim, rot}
    return mx.makeEntityNow("stack", "stack", obb)
end

function addItemToStack(item, stack)
    mx.addSymbolAttr(stack, "items", item)
    mx.setSymbolAttr(item, "inStack", stack)
end
