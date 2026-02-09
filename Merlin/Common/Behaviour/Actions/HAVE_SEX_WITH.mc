
action ?actorEnt HAVE_SEX_WITH ?female
    ->
(if (eq (attr ?female "pregnantWhen") @nothing) [
        (setAttr ?female "pregnantWhen" @now)
        (setAttr ?female "pregnantBy" ?actorEnt)
    ])
(setActionOutcome /succ).