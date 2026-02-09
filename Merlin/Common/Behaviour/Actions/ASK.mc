
action ?actorEnt ASK ?question ?personEnt 
    ->
# Create a sound right where the actor/speaker is
(attr ?actorEnt "obb"): ?actorObb
(makeEntity "sound" [k sound speech] ?actorObb (floats 400 400 400) (floats 0 0 0 1)): ?soundEnt
# Set the sound "createAction" to: "?actorEnt ASK ?question ?personEnt"
(extAction): ?questionAsk
(setAttr ?soundEnt "createAction" ?questionAsk)
# Associate the speaker with the sound
(setAttr ?soundEnt "speaker" ?actorEnt)
# The ASK action always succeeds.
(setActionOutcome /succ).
