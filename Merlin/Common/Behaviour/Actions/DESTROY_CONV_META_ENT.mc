
action ?actorEnt DESTROY_CONV_META_ENT ?convMetaEnt
    ->
# If someone else also in the conversation beat us to it,  we don't
# destroy the entitiy twice
(attr ?actorEnt "conversation"): ?curConv
(if (eq ?curConv ?convMetaEnt)
    # The destruction of the conv-entity clears all conversation attr values
    (destroyEntity ?convMetaEnt))

# Be aware that we're no longer in a conversation
(perceiveAttr ?actorEnt "conversation")

# The action always succeeds
(setActionOutcome /succ).