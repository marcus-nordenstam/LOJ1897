

# Reading docs in a stack relies on the "stackBrowse" activity 
# that makes us get docs from the stack, one at a time.
rule 
{@self stackRead ?stack}
{?stack isa [k object stack]}
    ->
(maintainProposal {@self stackBrowse ?stack}).


# If we are viewing a doc but have not read it, then read it
rule 
{@self stackRead ?stack}: ?stackRead
{@self stackBrowse ?stack /causes ~?stackRead}
# I have NOT read the doc I am currently viewing
{@self view ?doc}
    ->
# then try to read it
(maintainProposal {@self read ?doc}).


# If we are still viewing a doc that we have read, 
# put it into the 'done' stack.  
# (The stackBrowse activity creates this stack for us).
rule 
{@self stackRead ?stack}: ?stackRead
{@self stackBrowse ?stack /causes ~?stackRead}
# There is a stack where I can put the docs I am done with
{?stack doneStack ?doneStack}
# I have read the doc I am currently viewing
{@self view ?doc}
{@self /succ read ?doc}
    ->
# then try to dispose of it
(maintainProposal {@self stackPut ?doc ?doneStack}).


# If the stack is empty, end the activity /succ
rule 
{@self stackRead ?stack}: ?stackRead
{?stack isa [k object stack]}
{?stack top @nothing}
(none {@self view ?})
    ->
(setOutcome ?stackRead /succ).


rule 
{?doc fromStack ?stack}
{?stack doneStack ?doneStack}
{?hand control ?doc}
{@self hand ?hand}
{@self /ever /succ READ ?doc}
(none {@self stackRead ?stack})
    ->
(maintainProposal {@self stackPut ?doc ?doneStack}).


rule 
{?doc fromStack ?stack}
{?hand control ?doc}
{@self hand ?hand}
(none {@self /ever /succ READ ?doc})
(none {@self stackRead ?stack})
    ->
(maintainProposal {@self stackPut ?doc ?stack}).
