

rule learnBirthFamily
{@self mother ?mother}
{@self father ?father}
    ->
(o [k family] {@self family @o}): ?family
(beginBelief {?mother spouse ?father})
(beginBelief {?father spouse ?mother})
(beginBelief {?mother family ?family})
(beginBelief {?father family ?family})
#(print [@self has family members (any {@self familyMembers}).target])
(fireAndForget).


# We need to either make family exclusive, or
#   end {.. family @unknown}
#   maybe separate our immediate family from the spouse's side of the family...?

rule 
{@self spouse @something:?spouse}
    ->
(o [k family] {@self family @o}): ?family
(beginBelief {?spouse family ?family})
#(print [@self has family members (any {@self familyMembers}).target])
(fireAndForget).

rule 
{@self child ?child}
    ->
(o [k family] {@self family @o}): ?family
(beginBelief {?child family ?family})
#(print [@self has family members (any {@self familyMembers}).target])
(fireAndForget).

rule 
{@self sister ?sister}
    ->
(o [k family] {@self family @o}): ?family
(beginBelief {?sister family ?family})
#(print [@self has family members (any {@self familyMembers}).target])
(fireAndForget).

rule 
{@self brother ?brother}
    ->
(o [k family] {@self family @o}): ?family
(beginBelief {?brother family ?family})
#(print [@self has family members (any {@self familyMembers}).target])
(fireAndForget).

