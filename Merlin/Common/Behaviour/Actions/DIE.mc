
action ?actorEnt DIE 
    ->
(setAttr ?actorEnt "condition" dead)
(die)
(setActionOutcome /succ).
