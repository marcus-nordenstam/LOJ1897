
action ?actorEnt WEAR ?articleEnt ?bodyPartEnt
    ->
# Release ?articleEnt from whatever grips it
(setAttr ?articleEnt "controlForce" 0)
(attr ?articleEnt "controlledBy"): ?prevGripperEnt
(removeAttrItem ?prevGripperEnt "control" ?articleEnt)
(perceiveAttr ?prevGripperEnt "control")
# Make the body-part wear the article
(setAttr ?bodyPartEnt "wear" ?articleEnt)
(addAttrItem ?bodyPartEnt "control" ?articleEnt)
(perceiveAttr ?bodyPartEnt "wear")
(setAttr ?articleEnt "obb" /localPos 1 0 0)
(setAttr ?articleEnt "controlledBy" ?bodyPartEnt)
# WEAR successful
(setActionOutcome /succ).