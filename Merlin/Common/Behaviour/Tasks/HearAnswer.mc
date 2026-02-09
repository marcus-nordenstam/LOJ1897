
# --------------------------------------------------------------------------------------------------
# Rules for interpreting answers to questions
#
# Questions are always interpreted as if they were asked in the positive, because when people ask 
# questions, they expect an answer corresponding to the POSITIVE polarity of the question, 
# even when the question is posed in the negative.
#
# For example, "do you like shoes?" and "don't you like shoes?" are both treated as asking if you
# like shoes, despite the second version phrased with negative polarity.  If the answer is "no", 
# the interpretation is that you don't like shoes - in both cases.
# --------------------------------------------------------------------------------------------------


# This handles interpreting the answer to "is an event true?" questions.
#
# For example, the question "do you want to marry me?" is technically phrased as: 
#   "is it true that you want to marry me?"
# expressed as 
#   (qs (prob {@you want.task {@you marry @self}})).  
#
# If the answer is @true (e.g. "yes"), then the answer is understood by adding the belief:
#   {@you want.task {@you marry @self} /p @true}.
# If the answer is 0% (e.g. "no"), the resulting belief reflects that: 
#   {@you want.task {@you marry @self} /p @false}.
# If the answer is @unknown (e.g. "I don't know"), the resulting belief reflects that as well: 
#   {@you want.task {@you marry @self} /p unknown}.

rule believeIsEventTrueAnswer
{@self goal /succ {@self ASK '(qs (prob ?event)):?question ?person}}: ?askQuestionGoal
{@self /succ ASK ?question ?person /causes ~?askQuestionGoal}: ?askQuestionAction
{@self expect answer ?person /causes ~?askQuestionAction}
{?person /succ TELL ?answer @self /causes ~?askQuestionAction}: ?tellAnswer
    ->
# Now make a belief of the question-phrase (e.g. {@you want.task {@you marry @self}}), 
# with the probability equal to the answer
(beginBelief ?event [/p (msgContent ?answer) /sources ?tellAnswer]): ?beliefFromAnswer
#(print [@self believes ?beliefFromAnswer])
# You are no longer expecting an answer from this person
(endBelief {@self expect answer ?person}).
#(print [@self no longer expects answer from ?person]).


# This handles answers to questions about the target-value of some event, phrased
# as an "any" question with a target field-lookup.  
#
# For example, the question 
#   "what's your name?" 
# is expressed as: 
#   (qs (any {?person name}).target)
#  
# If the answer is "Toby", then to understand it, we need to splice together a new 
# belief out of the question & answer: 
#   {?person name "Toby"}.
rule believeTargetValueAnswer
{@self goal /succ {@self ASK '(qs (any ?event).target):?question ?person}}: ?askQuestionGoal
{@self /succ ASK ?question ?person /causes ~?askQuestionGoal}: ?askQuestionAction
{@self expect answer ?person /causes ~?askQuestionAction}
{?person /succ TELL ?answer @self /causes ~?askQuestionAction}
    ->
# Splice together a new belief:
# ?event = {?person name}, ?answer = "Toby" -> {?person name "Toby"}
(beginBelief ?event [/target (msgContent ?answer)]): ?understood
# You are no longer expecting an answer from this person
(endBelief {@self expect answer ?person}).
#(print [@self understands (nl ?understood)])
#(print [@self no longer expects answer from ?person]).


# Handles if the person asked doesn't know
rule believeIDontKnowAnswer
{@self goal /succ {@self ASK '(qs (any ?event).target):?question ?person}}: ?askQuestionGoal
{@self /succ ASK ?question ?person /causes ~?askQuestionGoal}: ?askQuestionAction
{@self expect answer ?person /causes ~?askQuestionAction}
# "I don't know" is represented as @fail.
{?person /succ TELL (msg @unknown) @self /causes ~?askQuestionAction}
    ->
(beginBelief {?person /not knowAnswer ?question})
# You are no longer expecting an answer from this person
(endBelief {@self expect answer ?person}).
#(print [@self no longer expects answer from ?person]).


# This handles interpreting the answer to "is object real?" questions.
#
# For example, the question "are you a laywer?" is technically phrased as: 
#   "is there a real lawyer-job that you have?"
# expressed as 
#   (qs (real {@you job @o} [k laywer])).  
#
# If the answer "yes", (e.g. @true) the answer is understood by adding a belief
# that they have that job.
rule believeIsObjectRealAnswer
{@self goal /succ {@self ASK '(qs (real ?descriptionOfRealObject)):?question ?person}}: ?askQuestionGoal
{@self /succ ASK ?question ?person /causes ~?askQuestionGoal}: ?askQuestionAction
{@self expect answer ?person /causes ~?askQuestionAction}
{?person /succ TELL (msg @true) @self /causes ~?askQuestionAction}
    ->
# ?descriptionOfRealObject will control events with @o's in them
(o /invent /real ?descriptionOfRealObject)
# You are no longer expecting an answer from this person
(endBelief {@self expect answer ?person}).
#(print [@self no longer expects answer from ?person]).
