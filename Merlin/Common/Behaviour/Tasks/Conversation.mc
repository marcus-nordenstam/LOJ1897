


# What is the reason for the conversation?
# It could be to ask a specific question.
# Or to chat with friend (which means learning new beliefs about them and telling new beliefs about me)
# So, conversations must have an agenda that lists states,goals, knowledge or whatever,
# and when fulfilled, the initiator of the conversation can end it.

# I WANT to have a private conversation with ?person
#{@self goal {@self conversation ?conv}}
#{?conv isa [k privateConv]}
#{?conv participant !@self:?person}

#-----------------------------------------------------------------------------------------
# If we want to have a conversation with someone, we must first determine if that person is 
# already in a conversation, or not.
#-----------------------------------------------------------------------------------------
rule startConv-perceive-conv-proposal /breakOnFire
{@self goal {@self conversation ?conv}}
{?conv participant !@self:?person}
(none {?person conversation}) # We don't know if ?person is already in a conversation
    ->
# If they're too far away to observe (or imaginary), then we could just always assume they're NOT in a conversation.
(maintainProposal {@self perceiveAttr ?person conversation}).


#-----------------------------------------------------------------------------------------
# STARTING A CONVERSATION
#-----------------------------------------------------------------------------------------

rule startConv-proposal
{@self goal {@self conversation ?irrConv}}
{@self conversation @nothing}
{?irrConv participant !@self:?person}
{?person conversation @nothing} # We know that ?person is NOT in ANY conversation
(lockRule 0) # Only try to start one conv at a time
    ->
(maintainProposal {@self startConv ?irrConv})
(print [@self wants to start a conversation with ?person]).


rule startConv-maintain-closeAndFacing-proposal
{@self startConv ?conv}
{?conv participant !@self:?person}
    ->
(maintainProposal {@self keepInReachOf ?person} /absUtil 1000)
(maintainProposal {@self keepFacing ?person} /absUtil 1000).


rule startConv-tell-howDo-proposal
{@self startConv ?conv}
{?conv participant !@self:?person}
    ->
(anyOrUnknown {?person name}).target: ?nameOrUnknown
(formulaic opening howDo ?nameOrUnknown): ?opening
(maintainGoal {@self TELL ?opening ?person}).


rule startConv-makeConvFromResponse-proposal
{@self startConv ?irrConv}: ?startConv
{?irrConv participant !@self:?person}
{@self /succ TELL ? ?person /causes ~?startConv}: ?openingTELL
{?person /succ TELL '(formulaic response howDo ?) @self /causes ~?openingTELL}
    ->
# Create the realis conversation (which is a meta-entity)
(beginProposal {@self MAKE_CONV_META_ENT ?person}).


# I am trying to start a conversation with ?person, 
# but ?person beats me to TELLing the formulaic opening
rule startConv-makeConvPreemptive-proposal
{@self startConv ?irrConv}: ?startConv
{?irrConv participant !@self:?person}
{?person /succ TELL '(formulaic opening ? ?) @self}
    ->
# Create the realis conversation (which is a meta-entity)
(beginProposal {@self MAKE_CONV_META_ENT ?person})
(break).


# NOTE that we don't need to end the goal to have the irrealis
# conversation since that goal is maintained only while we are not
# in a (realis) conversation with someone.  But if this rule fires,
# then we are now in a conversation with someone, so the goal will
# have already ended.
rule realise-conv
{@self /ever startConv ?irrConv}: ?startConv
{?irrConv participant !@self:?person}
{@self conversation @something:?reaConv}
{?person conversation ?reaConv}
    ->
# The conversation starting task was successful
(setOutcome ?startConv /succ)
# The irrealis conv is the same as the realis conv
# NOTE that this also ends all possessive ?irrConv beliefs
# and leads to ?irrConv being forgotten
(reconcile ?irrConv ?reaConv).

# Failure scenarios:

# Person tells me they're busy with another conversation.
# This could happen if multiple NPCs try to start a conversation at the same
# time with one person. 
rule startConv-outcome-fail /breakOnFire
{@self startConv ?irrConv}: ?startConv
{?irrConv participant !@self:?person}
{@self /succ TELL ? ?person /causes ~?startConv}: ?openingTELL
{?person /succ TELL '(formulaic refusal ? ?) @self /causes ~?openingTELL}
    ->
(setOutcome ?startConv /fail).

# if {@self initiateConversation ?person}
# and ?person declines your invitation in a rude manner (rude declining phrase, ignore, walk away)
# then
#   /begin ?person doesn't like you
#   (this realization may cancel the motivation to talk to them)

# if {@self initiateConversation ?person}
# ?person does NOT join another conversation (or any conversation)
# but informs you that they cannot talk now; they can talk later
# then
#   /begin {john availableToTalk /i @now+1h @now+2h} (which ends the previous availability state via contradiction)

#-----------------------------------------------------------------------------------------
# JOINING A CONVERSATION
#-----------------------------------------------------------------------------------------

/*
rule 
{@self goal {@self conversation ?conv}}
{?conv participant !@self:?person}
{?person conversation @something:?personsConv} # We know that ?person is in a conversation
(none {@self joinConv})
(lockRule 0) # Only try to join one conv at a time
    ->
(beginProposal {@self joinConv ?personsConv}).
*/

#-----------------------------------------------------------------------------------------
# RESPONDING TO A FORMULAIC OPENING (conversation starter)
#-----------------------------------------------------------------------------------------

rule conv-response-formulaicOpening-proposal
{@self conversation @nothing}
{?person /succ TELL '(formulaic opening howDo ?) @self}: ?personTell
(none {@self /succ TELL ? ?person /causes ~?personTell})
(lockRule 0) # Join only one conversation at a time
    ->
(anyOrUnknown {?person name}).target: ?nameOrUnknown
(formulaic response howDo ?nameOrUnknown): ?greeting
(beginGoal {@self TELL ?greeting ?person}): ?tellGreeting
(addCause ?tellGreeting ?personTell).


rule conv-response-refuseConv-proposal #/breakOnFire
{@self conversation @something:?conv}: ?talking
#{?person /succ TELL '(formulaic  opening ? ?) @self /beforeOrDuring ?talking}: ?personTell
{?person /succ TELL '(formulaic  opening ? ?) @self}: ?personTell
{?person conversation !?conv}
#(none {@self proposal {@self MAKE_CONV_META_ENT ?person}}) # don't refuse the person you are about to start a conversation with...
(none {@self /succ TELL ? ?person /causes ~?personTell})
    ->
(formulaic refusal ?talking): ?refusal
(beginGoal {@self TELL ?refusal ?person}): ?tellRefusal
(addCause ?tellRefusal ?personTell).

#-----------------------------------------------------------------------------------------
# CONVERSATIONAL BEHAVIOUR
#-----------------------------------------------------------------------------------------

rule conv-maintain-closeAndFacing-proposal
{@self conversation @something:?conv}
{!@self:?person conversation ?conv}
    ->
(maintainProposal {@self keepInReachOf ?person} /absUtil 1000)
(maintainProposal {@self keepFacing ?person} /absUtil 1000).


rule conv-todo-act-proposal
{@self conversation @something:?conv}
{?conv todo ?act /causes ?causes}
(lockRule 0) # only one todo-act at a time
    ->
(maintainProposal ?act): ?proposal
(addCause ?proposal ?causes) # add expl. cause because there are no task/goal conditions
(if (eq ?act.label ASK) 
    (beginBelief {@self expect answer ?act.auxiliary /causes ?act})).


rule conv-todo-act-complete
{@self conversation @something:?conv}
{?conv todo ?act /causes ?causes}: ?todoAct
# the proposed ?act will have a different pattern-ID from the todo ?act
# so we just use a wildcard and rely on the cause to connect them
{@self proposal ? /causes ~?causes /out?} 
    ->
(endBelief ?todoAct)
(print ["ENDING: " ?todoAct]).


#-----------------------------------------------------------------------------------------
# ENDING A CONVERSATION
#-----------------------------------------------------------------------------------------


# TODO:
#   Introduce a group object - which we use instead of lists
#   Introduce an 'audience' relation for conversations
#   Audience NEVER includes the @self, so it's a different group for each conversation member
#   That way we can deal with private vs group conversations without dealing with lists
#   Can also be used for all kinds of group activity rules
#   Example:
#       {conv audience aud}
#       {aud member sam}
#       {aud member bob}


# TODO: Don't end if the other person is displaying body-language 
# to the effect that they have more to say.

rule conv-end-proposal
{@self conversation @something:?conv}
{!@self:?audience conversation ?conv}
# Only let the person who initiated the conversation end it
{?conv initiator @self}
# Don't end a conversation if you're waiting on an answer
(none {@self expect answer ?audience})
# Don't end a conversation until you've said/done everything you intended
(print ["STILL KICKING:" (any {?conv todo})])
(none {?conv todo})
#(gt (evalCount) 20 /cont) # how many times this instruction has been evaluated since the rule activated
    ->
(break)
(maintainProposal {@self endConv ?conv} /relUtil 100).


rule endConv-destroyConv-proposal
{@self endConv ?conv}: ?endConv
{!@self:?person conversation ?conv}
    ->
(beginProposal {@self DESTROY_CONV_META_ENT ?conv} /relUtil 100)
# "we'll continue this later", "pardon me", "excuse me", "I have to go now", "bye", "see you later", etc.
(formulaic leaveTaking bye): ?leaveTaking
(beginGoal {@self TELL ?leaveTaking ?person} /relUtil 100).


rule endConv-outcome-succ
{@self endConv ?conv}: ?endConv
{!@self:?person conversation ?conv}
{@self /succ DESTROY_CONV_META_ENT ?conv}
    ->
(setOutcome ?endConv /succ).


# DEBUGGING RULES
/*
rule
{@self conversation @nothing}
    ->
(print ["--- " @self is NOT in a conversation]).

rule
{@self conversation @something}
    ->
(print ["+++ " @self IS in a conversation]).

rule
{!@self:?person conversation @nothing}
    ->
(print [" - " @self believes that ?person is NOT in a conversation]).

rule
{!@self:?person conversation @something}
    ->
(print [" + " @self believes that ?person IS in a conversation]).
*/

