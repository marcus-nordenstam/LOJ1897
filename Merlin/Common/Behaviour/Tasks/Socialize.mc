
# Social activities
#   AgeGroup 2+
#   Attend theatre (to provide a place where they can get to know others)
rule goTheatre
{@self alertness alert}
{@self spouse @nothing}
#{@self ageGroup >1}
{[k building theatre]:?theatre obb ?obb}
(in @self ?theatre /not /cont)
#(none {@self child})
    ->
(maintainProposal {@self go ?theatre} /cont).



/*
rule getAcquaintedWith
{@self ageGroup >1}
{@self gender ?gender}
{?person gender !?gender}
{?person ageGroup >1}
{@self family ?myFamily}
{?person family !?myFamily}
(lockRule 0) # only one firing of this rule at a time / mind
(real ?person)
(none {@self /succ getAcquaintedWith ?person}) # we don't know this person
    ->
(maintainProposal {@self getAcquaintedWith ?person}).

*/