

rule get-maintain-withinReachOf-proposal
{@self get ?thing}
    ->
(maintainProposal {@self keepInReachOf ?thing} /absUtil 1000)
(maintainProposal {@self keepFacing ?thing} /absUtil 1000).

rule get-take-proposal
{@self get ?thing}
{?thing obb !@unknown}
{@self hand ?hand}
{?hand control @nothing}
{@self withinReachOf ?thing}
{@self facing ?thing}
(real ?thing)
    ->
(maintainProposal {@self TAKE ?thing ?hand}).


# Base the activity's outcome on the corresponding action's outcome
rule get-outcome
{@self /ever get ?thing /noOut}: ?get
{@self /past TAKE ?thing /causes ~?get}: ?TAKE
    ->
(setOutcome ?get /from ?TAKE).

/*
rule 
{/prop @self TAKE ?thing ?freeHand}: ?TAKE
{?freeHand isa [k leftHand]}
    ->
(penalize ?TAKE 1).

rule 
{/prop @self TAKE ?thing ?freeHand}: ?TAKE
    ->
(penalize ?TAKE (distance @self ?thing /cont)).
*/