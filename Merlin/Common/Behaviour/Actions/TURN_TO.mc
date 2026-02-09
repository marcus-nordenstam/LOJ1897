
action ?actorEnt TURN_TO ?obb
    ->
(call turnTo ?actorEnt ?obb): ?result
#(perceiveAttr ?actorEnt "obb")
#(isFacing (internalize ?obb)): ?res
#(print ?res)
#(check ?res)
(if (eq ?result @true)
    (setActionOutcome /succ)).
