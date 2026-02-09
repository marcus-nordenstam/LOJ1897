
action ?actorEnt WALK_TO ?obb
    ->
(call walkTo ?actorEnt ?obb): ?result
(if (eq ?result @true) (setActionOutcome /succ)).
#(setAttr ?actorEnt "obb" ?obb /localPos)
