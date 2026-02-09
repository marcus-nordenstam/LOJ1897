
# Base Rule: If I want to learn if something is true about a person, I could ask the person.
#rule /cat learnProb /rank 0
rule
{@self goal {@self knowAnswer '(qs (prob {[k human]:?person ? ?})):?question}}
    ->
(maintainGoal {@self ASK ?question ?person})
(print [@self will ask (nl ?question) to ?person]).


# If a person has some relation to an object,
# and I'm trying to learn about that object,
# I can ask the person about it.
#rule 
#{@self goal {@self knowAnswer '(qs (real {? ? [k human]:?person})):?question}}: ?goal
#(msgContent ?question): ?qsContent
#(eqFuncName ?qsContent real)
#(tail ?qsContent).subject: ?subjects
#(any {?subjects isa [k human]}).subject: ?person
#    ->
#(maintainGoal {@self ASK ?question ?person}).

