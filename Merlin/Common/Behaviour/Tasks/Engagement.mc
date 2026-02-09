

rule goal-knowAnswerToMarriageProposal
{@self goal {@self marry ?prospect}}: ?iWantToMarryHer
{@self gender male}
{@self maritalState single}
#(none {?prospect goal {@prospect marry @self}})
#(gt (timeSince /cont /weeks ?iWantToMarryHer) 1) # had her in mind for 1 week
(lockRule 0) # synchronize the ability to activate this rule - only one at a time
    ->
(qs (prob {?prospect goal {?prospect marry @self}})): ?marriageProposal
(maintainGoal {@self knowAnswer ?marriageProposal})
(print [@self wants to know (nl ?marriageProposal)]).


rule reasoning-marriageProposalAccepted
{?proposer /succ ASK '(qs (prob {?proposee goal {?proposee marry ?proposer}})) ?proposee}: ?marriageProposal
{?proposee /succ TELL '(msg @true) ?proposer /causes ~?marriageProposal}
    ->
(beginBelief {?proposer fiancee ?proposee})
(beginBelief {?proposee fiancee ?proposer})
(fireAndForget).


rule reasoning-marriageProposalRejected
{?proposer /succ ASK '(qs (prob {!@self:?proposee goal {?proposee marry ?proposer}})) ?proposee}: ?marriageProposal
{?proposee /succ TELL '(msg @unknown%) ?proposer /causes ~?marriageProposal}
    ->
(beginBelief {?proposee goal {?proposee marry ?proposer} /p @unknown%})
(if (eq ?proposer @self) 
    [(setOutcome {@self goal {@self marry ?proposee}} /fail)
     (any {?proposee marriageDesirability}): ?desirabilityEvent
     (sub ?desirabilityEvent.target 0.1): ?newDesirability
     (beginBelief {?proposee marriageDesirability ?newDesirability})
     (print [@self updates desirability for ?proposee to ?newDesirability])])
(fireAndForget).



rule goal-possessEngagementRing
{@self goal {@self marry ?fiancee}}
{@self fiancee ?fiancee}
{@self gender female}
{?fiancee hand ?hand}
{?hand control [k engagementRing]:?ring}
    ->
(maintainGoal {@self possess ?ring})
(print [@self wants to possess ?ring]).


rule proposal-giveEngagementRing
{@self goal {@self marry ?fiancee}}
{@self fiancee ?fiancee}
{@self gender male}
{@self hand ?hand}
{?hand control [k engagementRing]:?ring}
(lockRule 0)
    ->
(maintainProposal {@self give ?ring ?fiancee}).


rule proposal-wearEngagementRing
{@self goal {@self marry ?fiancee}}
{@self fiancee ?fiancee}
{@self gender female}
{@self hand ?hand}
{?hand control [k engagementRing]:?ring}
{?fiancee /succ give ?ring @self}
{@self hand [k leftHand]:?lhand}
{?lhand finger [k ringFinger]:?ringFinger}
    ->
(beginBelief {?ring owner @self})
(maintainProposal {@self WEAR ?ring ?ringFinger}).
