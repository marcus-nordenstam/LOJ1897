

rule goHome
{@self home ?home}
{@self alertness sleepy}
(in @self ?home /not /cont)
    ->
(maintainProposal {@self go ?home}).