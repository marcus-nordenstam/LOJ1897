
action ?actorEnt STACK_TAKE ?thingEnt ?handEnt
    ->
# Get which stack ?thingEnt is in
(attr ?thingEnt "inStack"): ?stackEnt
# Physically remove ?thingEnt from that stack
(setAttr ?thingEnt "inStack" @nothing)
(removeAttrItem ?stackEnt "items" ?thingEnt)
# Update (and notice) the top item in ?stack
(setAttr ?stackEnt "top" (lastAttrItem ?stackEnt "items"))  
(perceiveAttr ?stackEnt "top")
# Set that ?thingEnt is now gripped by ?handEnt
(addAttrItem ?handEnt "control" ?thingEnt)
(setAttr ?thingEnt "controlledBy" ?handEnt)                      # This will cause the ?thingEnt OBB to be driven by ?handEnt OBB 
# Notice that actor is now holding & viewing ?thingEnt
(perceiveEntity ?thingEnt): ?mthing
(beginBelief {@self view ?mthing})
# Understand that ?thingEnt came from ?stackEnt
(perceiveEntity ?stackEnt): ?mstack
(beginBelief {?mthing fromStack ?mstack}) 
# Action was successful
(setActionOutcome /succ).