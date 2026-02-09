

# we are all npcs
#{@we role npc}

# we are all married
#{@we spouse @something}

# we are married to each other
#{@we spouse @weOther}

# we are related
#{@we xfamily ~@weOther}

# we are not related
#{@we xfamily ^@weOther}

/*
# grandparents

rule 
{@self [mother|father] ?parent}
{?parent mother ?grandMother}
    ->
(e {@self grandMother ?grandMother}).


rule 
{@self [mother|father] ?parent}
{?parent father ?grandFather}
    ->
(e {@self grandFather ?grandFather})


rule 
{@self [grandMother|grandFather] ?grandParent}
    ->
(e {@self grandParents ~?grandParent}).


# great grandparents

rule 
{@self [grandMother|grandFather] ?grandParent}
{?grandParent mother ?greatGrandMother}
    ->
(e {@self greatGrandMother ?greatGrandMother})


rule 
{@self [grandMother|grandFather] ?grandParent}
{?grandParent father ?greatGrandFather}
    ->
(e {@self greatGrandFather ?greatGrandFather})


rule 
{@self [greatGrandMother|greatGrandFather] ?greatGrandParent}
    ->
(e {@self greatGrandParents ~?greatGrandParent}).



# uncles

rule 
{@self [mother|father] ?parent}
{?parent brother ?uncle}
    ->
(e {@self uncle ?uncle}).


rule 
{@self [grandMother|grandFather] ?grandFather}
{?grandFather brother ?greatUncle}
    ->
(e {@self greatUncle ?greatUncle})



# aunts

rule 
{@self [mother|father] ?parent}
{?parent sister ?aunt}
    ->
(e {@self aunt ?aunt}).

rule 
{@self [grandMother|grandFather] ?grandFather}
{?grandFather sister ?greatAunt}
    ->
(e {@self greatAunt ?greatAunt})



# updating cousins

rule 
{@self [uncle|aunt] ?uncleOrAunt}
{?uncleOrAunt child ?cousin}
    ->
(e {@self cousin ?cousin}).


# Family members
rule 
{@self [father|mother|sister|brother|spouse|children] ?familyMember}
    ->
(e {@self family ~?familyMember}).


# Extended family members

{@self family ?family}
    ->
(e {@self xfamily ~?family}).

{@self [cousins|nephews|nieces|uncle|aunt|greatAunt|greatUncle|grandParents|greatGrandParents|grandChildren] ?xfamilyMember}
    ->
(e {@self xfamily ~?xfamilyMember}).

*/