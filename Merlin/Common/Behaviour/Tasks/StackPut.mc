
rule 
{@self stackPut ? ?stack}
{?stack isa [k object stack]}
    ->
(maintainProposal {@self keepInReachOf ?stack} /absUtil 1000).


rule 
{@self stackPut ?thing ?stack}
{?stack isa [k object stack]}
{@self hand ?hand}
{?hand control !?thing}
    ->
(maintainProposal {@self get ?thing}).


rule 
{@self stackPut ?thing ?stack}
{?stack isa [k object stack]}
{?hand control ?thing}
{@self hand ?hand}
{@self withinReachOf ?stack}
    ->
(maintainProposal {@self STACK_PUT ?thing ?stack}).


# Base the activity's outcome on the corresponding action's outcome
rule 
{@self /ever stackPut ?thing ?stack /noOut}: ?put
{@self /past STACK_PUT ?thing ?stack /causes ~?put}: ?PUT
    ->
(setOutcome ?put /from ?PUT).
