
# To read it, you must have it
rule 
{@self read ?doc}
{@self hand ?hand}
{?hand control !?doc}
    ->
(maintainProposal {@self get ?doc}).

rule 
{@self read ?doc}
{@self hand ?hand}
{?hand control ?doc}
    ->
(maintainProposal {@self READ ?doc}).


# Base the activity's outcome on the corresponding action's outcome
rule 
{@self /ever read ?doc /noOut}: ?read
{@self /past READ ?doc /causes ~?read}: ?READ
    ->
(setOutcome ?read /from ?READ).