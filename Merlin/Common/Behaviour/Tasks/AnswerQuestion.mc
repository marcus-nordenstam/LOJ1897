
# Questions are always modeled as FUNCTIONS phrased so that their output always produces the appropriate ANSWER.
#
# Question-functions may return a truthful or distored answer, by giving the /distort qualifier.
# Distoring the answer is how we implement lying.
#
# NOTE that the polarity of the question does NOT matter.
#
# For example, regardless if someone asks you "do you like shoes?" or "don't you like shoes?", 
# if your answer is "no", they will believe that you DONT like shoes in both cases.
#
# However, beliefs are always stored in the positive, regardless of the polarity of the source or 
# cause of the belief.

# The causal chain of someone answering a question is:
#    {john goal {john ASK question bob}}
#   *{john ASK question bob}
#   *{bob goal {bob TELL answer john}}
#    {bob TELL answer john}
# *=condition handled by this rule
rule respondToQuestion
{!@self:?person /ever ASK ?question @self}: ?personAsked
(none {@self /succ TELL ? ?person /causes ~?personAsked})
    ->
# Evaluate the question to produce an answer
(evalMsg /outputUnknownOnFail ?question): ?truthfulAnswer
# Set an absolute utility of 1, signifying that answering a question is higher priority than posing a question
(maintainGoal {@self TELL (msg ?truthfulAnswer) ?person} /absUtil 1): ?response
# Since the person asking is not an act performed by myself,
# we have to explicitly add it as a cause for my response.
(addCause ?response ?personAsked)
# Most rules declare at least one task among its conditions, which results in the rule not activating without
# said task-condition.  This rule, however, does *not* list any task since talking isn't something
# that requires a task.  So, to avoid a build-up of activated (but no longer firing) instances of this rule, 
# we use (forgetOnCease) to make sure the rule forgotten as soon as it ceases to fire.
(forgetOnCease).


# If someone asks a person if they want to do something, infer that someone wants the person to do it.
# For example if someone asks "do you want to go for a walk?", one would assume they want you to go for a walk.
rule infer-desireFromQuestion
{!@self:?someone /ever ASK '(qs (prob {?person goal {?person /task ? ?}:?task})) ?person}: ?question
    ->
(beginBelief {?someone goal ?task /sources ?question})
# As with the other rule above, this rule doesn't list a running task or goal in its header, so we manually 
# get rid of it once it has fired.
(fireAndForget).


# wanting:
#   communicative - asking if someone wants, telling that I want
#   reasoning - knowing what I want, and be able to answer questions about what I want


/* 
{?person ASK ?question @me}: ?personAsk
(none {@i SAY ? ?person /causes ~?personAsk})
    ->
(e ?question): ?truth
(e /distort ?question): ?lie
(util ?person learn ?truth): ?truthUtil
(util ?person learn ?lie): ?lieUtil
(if (ge ?truthUtil ?lieUtil)
    (maintainGoal {@i TELL ?truth ?person /causes ~?personAsk})
    (maintainGoal {@i TELL ?lie ?person /causes ~?personAsk})).
*/