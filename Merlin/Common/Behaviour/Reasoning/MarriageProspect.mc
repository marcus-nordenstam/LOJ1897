# TODO:
#   Generate a match-score based on
#       Attraction 
#       Suitability 
#       (For men only) Number of times asked with uncertain answer
#   Then have that be used in selecting the current marriage-prospect


rule inferMaritalState
{@self maritalState single}
{@self gender ?gender}
{[k human]:?person gender !?gender}
{?person ageGroup >0}
{?person maritalState @unknown}
{@self family ?myFamily}
{?person family !?myFamily}
    ->
#(maintainGoal {@self knowAnswer (qs (any {?person maritalState}).target)}).
(maintainProposal {@self infer {?person maritalState}}).


rule eligibleForMarriage
{@self maritalState single}
{@self gender ?gender}
{!@self:?person gender !?gender}
{?person condition alive}
{?person maritalState single}
{@self family ?myFamily}
{?person family !?myFamily}
    ->
(maintainBelief {?person eligibleForMarriage})
(if (none {?person marriageDesirability})
    (beginBelief {?person marriageDesirability (sub 1000 (id ?person))}))
(print [@self believes ?person is eligible for marriage])
(print [@self thinks ?person "has marriage-desirability" (any {?person marriageDesirability}).target]).


rule marry-desirability-decrease-longtimeNoSee
{@self goal {@self marry ?prospect}}
{?prospect marriageDesirability ?desirability}
(timeSinceObserved ?prospect /years /cont): ?timeSince
(ge ?timeSince 1)
    ->
(sub ?desirability ?timeSince): ?newDesirability
(beginBelief {?prospect marriageDesirability ?newDesirability}).


rule goal-marry
{@self maritalState !married}
(highest /target {? marriageDesirability ?}): ?desirabilityEvent
(lookup ?desirabilityEvent).subject: ?prospect
(lookup ?desirabilityEvent).target: ?desirability
    ->
#(every {? marriageDesirability ?}): ?every
#(print ?every)
(print [@self chooses ?prospect with ?desirability desirability])
#(log "pattern" on)
(maintainGoal {@self marry ?prospect})
#(log "pattern" off)
(check (gt ?desirability 0))
(check (eq 1 (count (every {@self goal {@self marry ?}})))).
