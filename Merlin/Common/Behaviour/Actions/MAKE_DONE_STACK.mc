
action ?actorEnt MAKE_DONE_STACK ?workingStackEnt ?doneStackObb
    ->
(makeEntity "stack" [k object stack] ?doneStackObb): ?doneStackEnt
# Stacks don't occlude anything
(setOccluderAttr ?doneStackEnt 0)
(perceiveEntity ?doneStackEnt): ?mdoneStack
(perceiveEntity ?workingStackEnt): ?mworkingStack
(beginBelief {?mworkingStack doneStack ?mdoneStack})
(setActionOutcome /succ).