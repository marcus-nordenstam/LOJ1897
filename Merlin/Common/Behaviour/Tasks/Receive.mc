
# Receive is similar to get, except it involves a giver, 
# whom we must be close to, and whose actions we must observe
# (get only tracks the item, since there is no giver)

rule receive-maintainWithinReachOf-proposal
{@self receive ?thing ?giver}
    ->
(maintainProposal {@self keepInReachOf ?giver} /absUtil 1000)
(maintainProposal {@self keepFacing ?giver} /absUtil 1000).


rule receive-take-proposal
{@self receive ?thing ?giver}
{?thing obb !@unknown}
{@self hand ?hand}
{?hand control @nothing}
{@self withinReachOf ?giver}
{@self facing ?giver}
(real ?thing)
    ->
(maintainProposal {@self TAKE ?thing ?hand}).


# Base the activity's outcome on the corresponding action's outcome
rule receive-outcome
{@self /ever receive ?thing /noOut}: ?receive
{@self /past TAKE ?thing /causes ~?receive}: ?TAKE
    ->
(setOutcome ?receive /from ?TAKE).

