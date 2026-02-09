
rule seekAnswers
{@self getAcquaintedWith ?person}
    ->
#(print [@self wants to get to know ?person])
[(qs (any {?person name}).target)
 (qs (any {?person mother}).target)
 (qs (any {?person father}).target)
 (qs (any {?person spouse}).target)
 (qs (any {?person home}).target)]: ?questions
(maintainProposal {@self seekAnswers ?questions}).


rule 
{@self /ever getAcquaintedWith ?person /noOut}: ?getAcquainted
{@self /past seekAnswers ? /causes ~?getAcquainted}: ?seekAnswers
    ->
(setOutcome ?getAcquainted /from ?seekAnswers).

