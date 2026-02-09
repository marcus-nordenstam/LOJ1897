
rule 
{@self stackGet ? ?stack}
{?stack isa [k object stack]}
    ->
(maintainProposal {@self keepInReachOf ?stack} /absUtil 1000).


rule 
{@self stackGet ?thing ?stack}
{?stack isa [k object stack]}
{@self hand ?hand}
{?hand control @nothing}
{@self withinReachOf ?stack}
    ->
(maintainProposal {@self STACK_TAKE ?thing ?hand}).


# Base the activity's outcome on the corresponding action's outcome
rule 
{@self /ever stackGet ?thing ?stack /noOut}: ?get
{@self /past STACK_TAKE ?thing ? /causes ~?get}: ?TAKE
    ->
(setOutcome ?get /from ?TAKE).
