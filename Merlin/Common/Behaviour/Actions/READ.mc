
# Reading a document is accomplished by perceiving the writings on the document
# followed by generating explicit beliefs out of them.
action ?actorEnt READ ?docEnt
    ->
# Perceive {?doc writings (msg content)} or 
#          {?doc writings (msg [content...])} 
(perceiveAttr ?docEnt "writings"): ?writingsEvent
# Then, form beliefs about the message contents:
(beginBelief (msgContent ?writingsEvent.target))
# The READ action always succeeds.
(setActionOutcome /succ).