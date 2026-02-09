
action ?actorEnt TAKE ?thingEnt ?handEnt
    ->
# If the thing is being gripped hard, this action fails
(attr ?thingEnt "controlForce"): ?curGripperForce
(if (eq ?curGripperForce 1)
    (setActionOutcome /fail))
# Release ?thingEnt from the hand that grips it
(attr ?thingEnt "controlledBy"): ?prevGripperEnt
(removeAttrItem ?prevGripperEnt "control" ?thingEnt)
(perceiveAttr ?prevGripperEnt "control")
# Set that ?thingEnt is now gripped by ?handEnt
(setAttr ?thingEnt "obb" /localPos 0 0 0)
(addAttrItem ?handEnt "control" ?thingEnt)
(setAttr ?thingEnt "controlledBy" ?handEnt) # This will cause the ?thingEnt OBB to be driven by ?handEnt OBB 
(setAttr ?thingEnt "controlForce" 1) 
# Notice that actor is now holding ?thingEnt
(perceiveEntity ?thingEnt): ?mthing
(perceiveAttr ?handEnt "control")
# TAKE successful
(setActionOutcome /succ).