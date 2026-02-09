
action ?actorEnt STACK_PUT ?thingEnt ?stackEnt
    ->
# Add ?thingEnt to the ?stackEnt
(setAttr ?thingEnt "inStack" ?stackEnt)                   # ?thing is now in ?stack
(addAttrItem ?stackEnt "items" ?thingEnt)                 # ?stack now has ?thing among its items
(setAttr ?stackEnt "top" ?thingEnt)                       # ?thing gets added as the top item in the stack
(perceiveAttr ?stackEnt "top")                                  # make the actor aware that ?thing is now the top item in ?stack
# Release ?thingEnt from the hand that grips it
(attr ?thingEnt "controlledBy"): ?handEnt
(removeAttrItem ?handEnt "control" ?thingEnt)
(perceiveAttr ?handEnt "control")
(setAttr ?thingEnt "controlledBy" @nothing)
# By putting ?thingEnt into a stack, I am no longer viewing it
(perceiveEntity ?thingEnt): ?thing
(endBelief {@self view ?thing})
# If ?thingEnt came from another stack, and that stack has a designated "done-stack", 
# and the stack we are putting ?thingEnt into is that "done-stack", then this action 
# is part of a browseStack activity -- requiring special handling: if the from-stack has 
# become empty, then we can swap the "from" and "done" stacks so that any future 
# browseStack activity can re-use the empty from-stack as the new done-stack, instead 
# of having to create a new, empty done-stack.
(any {?thing fromStack ?}):                 ?fromStackEvent
(any {?fromStackEvent.target top ?}):       ?fromStackTopEvent
(any {?fromStackEvent.target doneStack ?}): ?doneStackEvent
(perceiveEntity ?stackEnt): ?stack
(if (and (neq ?fromStackEvent           @fail)                      # there is a from-stack...
         (eq  ?fromStackTopEvent.target @nothing)                   # ..and it's empty...
         (eq  ?stack                    ?doneStackEvent.target)) [  # ...and the stack I'm putting ?thing into is the done-stack
    (beginBelief {?stack doneStack ?fromStackEvent.target}) 
    (forget ?doneStackEvent) 
    ])
# Wherever ?thing came from, it's now "in" the new stack
# and therefore it can no longer be "from" anywhere.
(endBelief  ?fromStackEvent)
# Action successful
(setActionOutcome /succ).
