

rule 
{@self write ?hypDoc}
{?hypDoc isa [k document]:?docKind}
{?hypDoc writings ?writings}
    ->
(maintainProposal {@self WRITE_DOC ?docKind ?writings}).


# Base the activity's outcome on the corresponding action's outcome
rule 
{@self /ever write ?hypDoc /noOut}: ?writeHypDoc
{@self /past WRITE_DOC ? ? /causes ~?writeHypDoc}: ?WRITE_DOC
#(o /only /known /causedBy ?WRITE_DOC): ?defDoc
(o /known /causedBy ?WRITE_DOC): ?defDoc
    ->
(setOutcome ?writeHypDoc /from ?WRITE_DOC)
# Replace "I wrote the hypothetical doc" with "I wrote the definite doc".
# NOTE that we are allowed to do this using (edit...) because the task we are 
# editing happened in the PAST, so does not need to be proposed.
(edit ?writeHypDoc [/target ?defDoc])
(forget ?hypDoc).


# TODO: Once I'm done writing, I should read it, but how I interpret it depends on
# the type of writing it is (non-fiction/fiction), and my knowledge of the author's intent:
#
# If the writings/msg is, and I don't know that the author is lying, then:
#   Non-fiction, then I should fire it as /factual /true
#   Fiction, then I should fire it as /factual /untrue
# If I know the author is lying then always fire as /factual /untrue
