
action ?actorEnt SPAWN ?entDescr 
    ->
(internalize ?entDescr): ?mdescr
(nth 0 ?mdescr): ?sysName
(nth 1 ?mdescr): ?kind
(nth 2 ?mdescr): ?pos
(nth 3 ?mdescr): ?radii
(nth 4 ?mdescr): ?orient
(makeEntity ?sysName ?kind ?pos ?radii ?orient): ?ent
(setOccluderAttr ?ent 0)
# If you spawn it, you know where it is
(perceiveAttr ?ent "obb")
# If you spawn it, you own it
(beginBelief {(perceiveEntity ?ent) owner @self})
(setActionOutcome /succ).
