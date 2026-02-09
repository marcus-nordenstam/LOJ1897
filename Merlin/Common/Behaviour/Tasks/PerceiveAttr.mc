
# Perceive a physical attribute on a physical object


# First of all, in order to perceive anything on an object, you'll
# have to be close enough to observe/hear/smell it
rule 
{@self perceiveAttr ?thing ?attr}: ?perceiveAttr
    ->
(maintainProposal {@self keepInReachOf ?thing} /absUtil 1000).


# If you are close enough, and you have perceived the object, then
# we can propose to perform the actual PERCEIVE_ATTR action which
# lets us perceive the specific attribute on the object.
rule 
{@self perceiveAttr ?thing ?attr}: ?perceiveAttr
(isPerceived ?thing) # You can't actually perceive any attr on ?thing until you have seen ?thing
    ->
(maintainProposal {@self PERCEIVE_ATTR ?thing ?attr}).

rule 
{@self /ever perceiveAttr ? ? /noOut}: ?perceiveAttr
{@self /past PERCEIVE_ATTR ? ? /causes ~?perceiveAttr}: ?PERCEIVE_ATTR
    ->
(setOutcome ?perceiveAttr /from ?PERCEIVE_ATTR).
