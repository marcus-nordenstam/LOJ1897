
action ?actorEnt MAKE_CONV_META_ENT ?personEnt
    ->
# If it turns out, by the time this action runs, 
# that ?actorEnt is already in a conversation, 
# then this action is unneccessary (and benignly fails) 
# because their conversation partner beat them to it.
(attr ?actorEnt "conversation"): ?curConv
(if (eq ?curConv @nothing)
     # Create a new conversation meta-entity
    [(makeEntity "conv" [k conversation]): ?convMetaEnt
     # Set the conversation on each participant
     (setAttr ?actorEnt "conversation" ?convMetaEnt)
     (setAttr ?personEnt "conversation" ?convMetaEnt)
     # Set who started the conversation
     (setAttr ?convMetaEnt "initiator" ?actorEnt)])

# Be aware that ?personEnt and ?personEnt now have the same conversation
(perceiveAttr ?actorEnt "conversation")
(perceiveAttr ?personEnt "conversation")

# The action always succeeds
(setActionOutcome /succ).
