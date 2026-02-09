

# Unlike ASK, TELL can be done inside or outside of a conversation.

# These first two rules handle outside the converation:
rule goal-conv-TELL-notAvail
{@self goal {@self TELL ?msg ?audience}}: ?goal
{@self conversation ?myConv}
{?audience conversation !?myConv}
    ->
(maintainProposal {@self keepInReachOf ?audience} /absUtil 1000)
(maintainProposal {@self keepFacing ?audience} /absUtil 1000).


rule goal-conv-TELL-bothAvail
{@self goal {@self TELL ?msg ?audience}}: ?goal
{@self conversation @nothing}
{?audience conversation @nothing}
    ->
(maintainProposal {@self keepInReachOf ?audience} /absUtil 1000)
(maintainProposal {@self keepFacing ?audience} /absUtil 1000).


# This rule handles the case where we want to say something to someone we're currently speaking with
rule conv-TELL-todo
{@self goal {@self TELL ?msg ?audience}}: ?goal
{@self conversation @something:?myConv}
{?audience conversation ?myConv}
    ->
(beginBelief {?myConv todo {@self TELL ?msg ?audience} /causes ?goal}). # add explicit cause because todo is state


rule TELL-proposal
{@self goal {@self TELL ?msg ?audience}}
{@self withinReachOf ?audience}
{@self facing ?audience}
(lockRule 0) # only be telling one thing at a time
    ->
(maintainProposal {@self TELL ?msg ?audience}).
#(print [@self proposes to say (nl ?message) because (nl ?causes)]).


rule goal-TELL-outcome
{@self goal {@self TELL ?msg ?audience}}: ?goal
{@self TELL ?msg ?audience /causes ~?goal /out?}: ?TELL
    ->
(setOutcome ?goal /from ?TELL).
