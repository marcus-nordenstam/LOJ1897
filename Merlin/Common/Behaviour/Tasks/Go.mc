

# NOTE that going to and locating documents must be done differently
# since documents are in stacks; so these rules only apply for non-documents destinations.

rule go-locate-proposal
#{@self go ![k document]:?dest} 
{@self go ?dest} 
{?dest obb @unknown}
    ->
(maintainProposal {@self locate ?dest}).


rule go-walkTo-proposal
#{@self go ![k document]:?dest}
{@self go ?dest}
{?dest obb !@unknown:?obb}
    ->
(maintainProposal {@self WALK_TO ?obb}).


# Base the activity's outcome on the corresponding action's outcome
rule go-outcome
#{@self /ever go ![k document]:?dest /noOut}: ?go
{@self /ever go ?dest /noOut}: ?go
{?dest obb ?obb}
{@self /past WALK_TO ?obb /causes ~?go}: ?WALK
    ->
(setOutcome ?go /from ?WALK).


