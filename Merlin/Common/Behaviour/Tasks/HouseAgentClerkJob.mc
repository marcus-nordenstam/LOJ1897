
rule 
{@self perform [k clerk]:?job}: ?working
{?job at [k houseAgency]:?org}
{?buyer goal ?otask}
{?otask task {?buyer buy [k building]:?bldg}}: ?buyerWantsToBuy
(real ?bldg)
    ->
# We have to explicitly include ?buyerWantsToBuy as a cause, because it is NOT a self-act
(maintainProposal {@self processTitleDeed ?buyer ?bldg /causes ?buyerWantsToBuy}).

rule 
{@self processTitleDeed ?buyer ?bldg}
(none {?buyer name ?})
    ->
(qs (any {?buyer name}).target): ?qsWhatIsBuyersName
(maintainGoal {@self knowAnswer ?qsWhatIsBuyersName} /relUtil 1).


rule 
{@self processTitleDeed ?buyer ?bldg}
{?bldg isa [k building]:?bldgKind}
{?buyer name ?buyerName}
{?bldg name ?address}
    ->
(msg {(o ?bldgKind ?address) owner (o {@o name ?buyerName})}): ?writings
(o /invent /hyp [k titleDeed] {@o writings ?writings}): ?hypTitleDeed
(maintainProposal {@self write ?hypTitleDeed}).


# Give the doc to buyer
rule 
{@self processTitleDeed ?buyer ?bldg}: ?processTitleDeed
{@self /succ write [k titleDeed]:?doc /causes ~?processTitleDeed}
{?buyer hand ?hand}
(real ?doc)
(none {?hand /ever control ?doc})
    ->
# Once give succeeds, then (endBelief  buyer-goal-to-buy...)
(maintainProposal {@self give ?doc ?buyer}).


# If someone wants to buy a property,
# and I've done my part (created a title-deed and given it to them)
# then 
