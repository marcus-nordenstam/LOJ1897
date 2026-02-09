
action ?actorEnt TELL ?message ?personEnt 
    ->
# Create a sound right where the actor/speaker is
(attr ?actorEnt "obb"): ?actorObb
(makeEntity "sound" [k sound speech] ?actorObb (floats 40 40 40) (floats 0 0 0 1)): ?soundEnt
# Set the sound "createAction" to: "?actorEnt TELL ?message ?personEnt"
(extAction): ?tellMsg
(setAttr ?soundEnt "createAction" ?tellMsg)
# Associate the speaker with the sound
(setAttr ?soundEnt "speaker" ?actorEnt)
# The ASK action always succeeds
(setActionOutcome /succ).