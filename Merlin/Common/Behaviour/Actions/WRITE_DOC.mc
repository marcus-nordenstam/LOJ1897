
action ?actorEnt WRITE_DOC ?docKind ?writings
    ->
# Create the document right where the actor is
(attr ?actorEnt "obb"): ?actorObb
(makeEntity "prop" ?docKind ?actorObb): ?docEnt
# Documents are too small to occlude anything
(setOccluderAttr ?docEnt 0)
# Add the given writings to the doc
(setAttr ?docEnt "writings" ?writings)
# The cause of the document is this action
(addCause (perceiveEntity ?docEnt) (intAction))
# Action successful!
(setActionOutcome /succ).
