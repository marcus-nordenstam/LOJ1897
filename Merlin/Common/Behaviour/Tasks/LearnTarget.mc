
# Base Rule: If I want to learn some general aspect about a person, I could ask the person.
#rule /cat learnTarget /rank 0
rule
{@self goal {@self knowAnswer '(qs (any {[k human]:?person ? ?}).target):?question}}
   ->
(maintainGoal {@self ASK ?question ?person}).

# Specific versions of the knowTarget rule -- overrides base rule:

#rule /cat learnTarget /rank 1
#{@self goal {@self knowAnswer '(qs (any {?person maritalState}).target)}}
#    ->
#(maintainProposal {@self infer {?person maritalState}}).
