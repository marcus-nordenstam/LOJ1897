
# Marriage starts once the groom has acquired a new home

rule seekHomeForSpouse
{@self fiancee @something:?fiancee}
{@self gender male}
{@self parent ?parent}
{?parent condition alive}
{?parent home ?parentHome}
{@self home ?parentHome}
(lockRule 0)
    ->
(print [@self begins seeking a home])
(beginGoal {@self home (o /invent [k residentialBuilding])}).


rule acquireHome
{@self goal {@self home ?irrHome}}
{?irrHome isa [k building]:?bldgKind}
    ->
(print [@self will acquire ?bldgKind])
(maintainProposal {@self ACQUIRE ?bldgKind}).


rule acquireHomeOutcome
{@self goal {@self home ?irrHome}}: ?goal
{?irrHome isa [k building]:?bldgKind}
{@self /succ ACQUIRE ?bldgKind /causes ~?goal}
    ->
(print [@self no longer seeks a home])
(setOutcome /succ ?goal).



rule marry-proposal
{@self goal {@self marry ?fiancee}}
{@self fiancee ?fiancee}
{@self gender male}
{@self parent ?parent}
{?parent condition alive}
{?parent home ?parentHome}
{@self home !?parentHome:?myHome}
{?myHome obb ?myHomeObb}
(lockRule 0)
    ->
(beginProposal {@self marry ?fiancee})
(beginProposal {@self SPAWN ["prop" [k weddingBand] @self (floats 0.2 1 1) (floats 0 0 0 1)]})
(beginGoal {@self TELL (msg {@self home ?myHome}) ?fiancee})
(beginGoal {@self TELL (msg {?myHome obb ?myHomeObb}) ?fiancee}).


rule marry-attention
{@self marry ?fiancee}
    ->
(maintainAttention ?fiancee).
#(maintainProposal {@self keepFacing ?fiancee} /absUtil 1000).
#(maintainProposal {@self keepInReachOf ?fiancee} /absUtil 1000)


rule marry-giveWeddingBand
{@self marry ?fiancee}
{@self gender male}
{[k weddingBand]:?weddingBand owner @self}
{?fiancee hand [k leftHand]:?lhand}
{?lhand finger [k ringFinger]:?ringFinger}
(none {?ringFinger wear [k weddingBand]})
    ->
(beginBelief {?weddingBand owner ?fiancee})
(beginProposal {@self give ?weddingBand ?fiancee})
(print [@self will give ?weddingBand to ?fiancee]).


# BRIDE behaviour
rule female-marry
{@self fiancee @something:?fiancee}
{@self gender female}
{?fiancee OFFER [k weddingBand]:?weddingBand @self}
    ->
(beginProposal {@self marry ?fiancee})
(maintainGoal {@self possess ?weddingBand})
(print [@self wants to possess ?weddingBand]).


rule female-marry-wearWeddingBand
{@self marry ?fiancee}
{?fiancee home ?fianceeHome}
{@self gender female}
{@self hand ?hand}
{?hand control [k weddingBand]:?ring}
{?fiancee /succ give ?ring @self}
{@self hand [k leftHand]:?lhand}
{?lhand finger [k ringFinger]:?ringFinger}
    ->
(beginBelief {?ring owner @self})
(maintainProposal {@self WEAR ?ring ?ringFinger})
(beginBelief {@self home ?fianceeHome})
(print [@self wants to put on weddingBand])
(print (every {?fiancee /ever give})).


# GROOM behaviour
rule conjureGroomWeddingBand
{@self marry ?fiancee}
{?fiancee hand [k leftHand]:?lhand}
{?lhand finger [k ringFinger]:?ringFinger}
{?ringFinger wear [k weddingBand]}
    ->
(beginProposal {@self SPAWN ["prop" [k weddingBand] @self (floats 0.2 1 1) (floats 0 0 0 1)]}).

rule groomWearWeddingBand
{@self marry ?fiancee}
{?fiancee hand [k leftHand]:?lhand}
{?lhand finger [k ringFinger]:?ringFinger}
{?ringFinger wear [k weddingBand]}
{[k weddingBand]:?myRing owner @self}
{@self hand [k leftHand]:?myLeftHand}
{?myLeftHand finger [k ringFinger]:?myRingFinger}
(none {?myLeftHand wear ?myRing})
    ->
(maintainProposal {@self WEAR ?myRing ?myRingFinger}).


# Rules for both BRIDE & GROOM
rule marriedSuccess
{@self marry ?fiancee}: ?marry
{?fiancee hand [k leftHand]:?lhand}
{?lhand finger [k ringFinger]:?ringFinger}
{?ringFinger wear [k weddingBand]}
{@self hand [k leftHand]:?myLeftHand}
{?myLeftHand finger [k ringFinger]:?myRingFinger}
{?myRingFinger wear [k weddingBand]}
    ->
(setOutcome /succ ?marry)
(endBelief {@self fiancee ?fiancee})
(beginBelief {@self spouse ?fiancee})
(print [@self and ?fiancee are married]).



# PUTTING PARENTS HOUSE BACK ON THE MARKET






/*

rule 
{@self fiancee @something:?fiancee}
{@self gender male}
{?weddingBand isa [k weddingBand]}
{@self hand [k leftHand]:?lhand}
{@self hand [k rightHand]:?rhand}
(none {? control ?weddingBand})
    ->
(maintainProposal {@self take ?weddingBand}).

*/