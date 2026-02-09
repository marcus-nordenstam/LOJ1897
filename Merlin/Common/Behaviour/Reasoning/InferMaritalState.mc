
# I can infer the martial status of anyone by considering the known spouse/fiancee relationships

rule inferSingle
{?person fiancee @nothing}
{?person spouse @nothing}
#{?person ageGroup >2}
{?person ageGroup >0}
    ->
(beginBelief {?person maritalState single})
(print [@self infers that ?person is single])
(fireAndForget).

rule inferEngaged
{?person fiancee @something}
{?person spouse @nothing}
    ->
(beginBelief {?person maritalState engaged})
(print [@self infers that ?person is engaged])
(fireAndForget).

rule inferMarried
{?person spouse @something}
    ->
(beginBelief {?person maritalState married})
(print [@self infers that ?person is married])
(fireAndForget).


# You can infer spouse/fiancee relationships of OTHERS by examining the rings on the person's left hand ringfinger

# If the person is a female and NOT wearing an engagement-ring, then she's NOT engaged
# If the person is male, well, they never wear engagement-rings, so we'll just optimistically believe they're NOT engaged as well.
rule inferNoFiancee
{@self infer {?person maritalState}}
{?person hand [k leftHand]:?lhand}
{?lhand finger [k ringFinger]:?ringFinger}
(none {?ringFinger wear [k engagementRing]})
    ->
(beginBelief {?person fiancee @nothing}).


# If the person, regardless of gender, is NOT wearing a wedding-band, then they're NOT married
rule inferNoSpouse
{@self infer {?person maritalState}}
{?person hand [k leftHand]:?lhand}
{?lhand finger [k ringFinger]:?ringFinger}
(none {?ringFinger wear [k weddingBand]})
    ->
(beginBelief {?person spouse @nothing}).


# If the person is female and wears an engagement-ring (but no wedding-band), then she's engaged
rule inferFiancee
{@self infer {?person maritalState}}
{?person gender female}
{?person hand [k leftHand]:?lhand}
{?lhand finger [k ringFinger]:?ringFinger}
{?ringFinger wear [k engagementRing]}
(none {?ringFinger wear [k weddingBand]})
    ->
(beginBelief {?person fiancee @something})
(beginBelief {?person spouse @nothing}).


# If the person, regardless of gender, wears a wedding-band, then they're married, and NOT engaged (anymore)
rule inferSpouse
{@self infer {?person maritalState}}
{?person hand [k leftHand]:?lhand}
{?lhand finger [k ringFinger]:?ringFinger}
{?ringFinger wear [k weddingBand]}
    ->
(beginBelief {?person fiancee @nothing})
(beginBelief {?person spouse @something}).

