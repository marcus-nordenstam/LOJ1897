

# Pay attention to the (initial) stack level (so we know if its empty or not)
# NOTE that the '?stack top' event is updated by the STACK_TAKE and STACK_PUT actions
# so we only ever have to explicitly observe it at the start of the activity, hence this rule.
rule 
{@self stackBrowse ?stack}: ?browseStack
    ->
(maintainProposal {@self perceiveAttr ?stack top}).


# We can't start this activity if we're already gripping objects because we don't want to confuse those
# with the objects from the stack that we will be browsing.  So just drop any objects we're holding.
rule 
{@self stackBrowse ?stack}: ?stackBrowse
{@self hand ?hand}
# Any objects taken from the stack will have the 'fromStack' state on them
# (because STACK_TAKE adds that state)
(o /known {?hand control @o} {@o /not fromStack ?stack}): ?thing
    ->
(maintainProposal {@self drop ?thing}).


# This rule prevents us from greedily getting multiple docs at a time.
rule 
{@self stackBrowse ?stack}
# the stack is NOT empty
{?stack top @something:?doc}
# and I am NOT gripping anything
{@self hand ?hand}
{?hand control @nothing}
    ->
# then set a goal to get the next doc from the stack
(maintainProposal {@self stackGet ?doc ?stack}).



