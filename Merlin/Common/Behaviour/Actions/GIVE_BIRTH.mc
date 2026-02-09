
action ?motherEnt GIVE_BIRTH
    ->
(call giveBirth ?motherEnt): ?child
# The mother is no longer pregnant
(setAttr ?motherEnt "pregnantWhen" @nothing)
(setAttr ?motherEnt "pregnantBy" @nothing)
(setActionOutcome /succ).