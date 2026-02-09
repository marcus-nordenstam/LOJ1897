
# NOTES about message polarity and beliefs that arise from them.
#
# Messages may use negation to express polarity, but beliefs are always
# stored in the positive.  Instead of setting polarity to negative, we 
# invert he probabilty of the belief, as follows:
#
# If I am told: 
#   {/msg {john} /not like [k shoe]}
# and I believe the speaker, then I will believe: 
#   {{john} like [k shoe] /p @false}
#
# Likewise, if I am told:
#   {/msg {john} /not like [k shoe]}
# but I do NOT believe the speaker, then I will believe: 
#   {{john} like [k shoe] /p @true}
#
# And if I'm told a positive:
#   {/msg {john} like [k shoe]}
# but I do NOT believe the speaker, then I will believe: 
#   {{john} like [k shoe] /p @false}


# This handles the general case where someone makes a verbal statement, 
# but it's NOT in response to anything.
rule hear-tell
{!@self:?person /succ TELL ?msg ? /causes []}
    ->
(beginBelief (evalMsg ?msg))
(fireAndForget).


#rule someone_want_self_action
#{!@self:?someone goal {/action @self ? ?}:?action}
#    ->
#(maintainProposal ?action).

#rule someone_want_self_task
#{!@self:?someone goal {/task @self ? ?}:?task}
#    ->
#(maintainProposal ?task).


#rule tell_about
#{@self tell_about ?x ?someone}
#    ->
#(break).


# If any personality trait > "average threshold", then you'd report it
#(every {?x interest})

 