
# We need a 'done' stack to put each item in when we are done viewing it
rule 
{@self stackBrowse ?stack}
{?stack obb ?obb}
(none {?stack doneStack ?})
    ->
(maintainProposal {@self makeDoneStack ?stack ?obb}).


rule 
{@self makeDoneStack ?workingStack ?doneStackObb}
(isPerceived ?workingStack) # We need to have observed the ?stack in order to make a done-stack for it.
    ->
(maintainProposal {@self MAKE_DONE_STACK ?workingStack ?doneStackObb}).
