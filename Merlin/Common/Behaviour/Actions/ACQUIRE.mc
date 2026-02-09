
action ?actorEnt ACQUIRE ?bldgKind
    ->
(call acquireBuilding ?bldgKind): ?bldgEnt
(perceiveEntity ?bldgEnt): ?bldg
(perceiveAttr ?bldgEnt "obb")
(print [@self acquired ?bldg])
(beginBelief {@self home ?bldg})
(setActionOutcome /succ).