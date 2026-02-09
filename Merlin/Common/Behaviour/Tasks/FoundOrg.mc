

# Founders must set a goal to found their orgs
rule 
{@self mustFoundOrg ?org}
    ->
(maintainProposal {@self foundOrg ?org}).

# TODO: When I succeed in foundOrg ?org,
#       because I have created a /def org ?defOrg, then
#       manually reconcile /des -> /def org.
rule 
{@self foundOrg ?org}
{?org workplaceKind ?workplaceKind}
(none {?org workplace ?})
(o /realOrHyp ?workplaceKind {@o availability forSale}): ?workplace
    ->
(maintainProposal {@self buy ?workplace}).


# to obtain a workplace I must go to the house agent and search their stack of available properties
# then I must find one that matches the workplace kind
# then I must tell the agent I want the matching property


/*
rule 
{?doc isa [k titleDeed]}
(none {@self READ ?doc})
    ->
(maintainProposal {@self READ ?doc}).



rule 
{@self job ?job}
{?job profName clerk}
{?job org ?org}
{?org isa [k landRegistry]}
{?founder goal {.. achieve {?clientOrg workplace ?workplaceDescr}}}
{?clientOrg name ?ownerName}
{?clientOrg categories ?ownerCats}
{@time date ?date}
# Do I know of a place that matches the ?clientOrg's workplace description?
(o /known ?workplaceDescr): ?place
# Determine if this place is listed in a deed, and that we have directly
# observed that deed
(src ?place): ?deed
(any {/obs ?deed kind titleDeed})
# If so, then extract the address and categories of the place
(has ?place ; name ?address ; cats ?placeCats)
    -> 
# If we get here, then ?deed is the mental symbol for the title-deed that lists 
# ?place as available for purchase.
# Furthermore, ?deed has been directly observed (presumably by reading it)
# which means its externalization will be the world-entity symbol corresponding 
# to the actual deed-document -- which is required in order to perform actions on it,
# such as the update we do now:
(call "updateTitleDeed" @self ?deed ?placeCats ?address ?ownerCats ?ownerName ?date)
(maintainProposal {@self give ?deed ?founder}).

# Creating org stacks
# Knowing they belong to the org
# Knowing the org workplace
# Moving them to the workplace

# The very first requirement is always assumed to be cats, if:
#   it only has one non-qualifier symbol (which could be a list)  

# n uses the name to infer /the and its category.


# Shorthand for querying or defining possessive events:
(n [John Smith] ; date Apr-18-1897).property
# which is the same as the possessive event:
{(n [John Smith] ; date Apr-18-1897) property (o property)}


# Expressing information in a single sentence:

# "John Smith's property includes the building at '14 Victoria Ave'"
#
# Which can be written compactly as follows, taking full advantage of the ontology, which tells us:
# (1) the target of the 'buildings' relation is expected to be a building
# (2) the subject of the 'property' relation is expected to be a person or organisation, and
#     the hstr-list [John Smith] contains no terms used to name an organisation, so it must be a person. 
{(property [n John Smith]) buildings ~[n 14 Victoria Ave]}

# Rephrased as: the property of John Smith, born on Apr-18-1897, includes the building at "14 Victoria Ave"
{(property (o [n John Smith] [date Apr-18-1897])) buildings ~[14 Victoria Ave]}

# Expressing the same information as two separate sentences:
[
    (property (o [n John Smith] [date Apr-18-1897]))  # John Smith, born on April 18, 1897, has/owns property. 
    {@it buildings ~[14 Victoria Ave]}              # It includes "14 Victoria Ave".
] 

# The own/owner functions can work as follows:
# If it is known that someone owns something, we are done (check if a matching event exists)
# If not, the function tries to reason about it.  For example, if the object in question is IN a building that the person owns.
# Either way, by ASKING if someone owns something, or by STATING that someone owns something, we are in effect
# telling the mind that this particular ownership is important, so create/update an event accordingly.  That way we only
# generate ownership events on an as-neede basis and we don't force the mind to create ownership events for everything it sees.

# Another example.  If we represent ownership explicitly as an event.  However, if these are rule-conditions, 
# then it only works if we (the rule-writer) KNOWS that this particular ownership IS an event.
[
    {[n 14 Victoria Ave] owner [n John Smith]}          # '14 Victoria Ave' is owned by John Smith.
    {@his date Apr-18-1897}                         # He was born on April 18, 1897
] 

# Same as above, but ownership is now a function instead of an event.  As rule-conditions, this is safer since
# it works regardless of if this particular ownership is an event or not.
[
    (owner [n 14 Victoria Ave] [n John Smith])          # '14 Victoria Ave' is owned by John Smith.
    {@his date Apr-18-1897}                         # He was born on April 18, 1897
] 

# Same as above, except given as a single sentence:
# '14 Victoria Ave' is owned by John Smith, who was born on April 18, 1897.
(owner [n 14 Victoria Ave] (o [n John Smith] [date Apr-18-1897]))

# Flipped around:
# John Smith owns '14 Victoria Ave', which was constructed on April 18, 1897.
(own [John Smith] (o [n 14 Victoria Ave] [date Apr-18-1897]))  

# NOTE how in these examples, both owner and own functions returns the argument indicated by the function-name.

# Adding tense:
# John Smith owned '14 Victoria Ave' during April 18, 1897.
(own [John Smith] [14 Victoria Ave] /during Apr-18-1897)  


# Title deed of ownership
{[n ?addr] owner [n ?owner] /i ?date @ongoing}
{[n ?addr] owner @nothing /i ?date @ongoing}

# Documents are structured as follows:
#
# doc isa       kind
# doc writings  [[sectionKind sentences] [sectionKind sentences] ...]
#
# writings is a list of sections
# each section is itself a list: [sectionKind sentences] 
# sectionKind is a kind-symbol telling us the kind of section (chapter, declaration, etc)
# all remaining symbols in the list are expected to be the written sentences.
#
# So a title deed stating that John Smith owns 14 Victoria Ave, would be encoded as follows:
#
{{deed} isa      [k object document titleDeed]}
{{deed} writings [[k declaration] {[n 14 Victoria Ave] owner [n John Smith] /i ?date @ongoing}]}



# All questions are functions:
# 
# If a function is by definition a query, then it is always a question:
# "What is your name?"
(what {@you name ?})
# "When did you last see John?" 
(max {@you observe john}.end)
# Who owns '14 Victoria Ave'?
(who {[n 14 Victoria Ave] owner ?})

# Some functions can be interpreted as a statement OR a question.  For example, (own...) can be
# used to state that someone owns something, or ask if someone owns something.  In the latter case
# we simply append /qs to indicate the question-form of the function.  Here are examples of this:

# John Smith, born on April 18, 1897, owned '14 Victoria Ave' during the month of March, 1901.
(own (o [n John Smith] [date Apr-18-1897]) [n 14 Victoria Ave] [/during March-1901])

# Did John Smith, born on April 18, 1897, own '14 Victoria Ave' during the month of March, 1901?
(own (o [n John Smith] [date Apr-18-1897]) [n 14 Victoria Ave] [/during March-1901] /qs)



# To avoid having to implement pattern-matching on functions (for now at least), and for general
# efficiency reasons, we differentiate between statements/imperatives and questions by using
# Tell for statements and imperatives, vs Ask for questions.
#
# The target of the Ask relation is always a function.

{?person Ask ?question @me}: ?personAsk
(none {@i TELL ?question ?person /motivatedBy ?personAsk})
    ->
(maintainBelief ?question): ?truth
(maintainBelief /distort ?question): ?lie
(util ?person learn ?truth): ?truthUtil
(util ?person learn ?lie): ?lieUtil
(if (ge ?truthUtil ?lieUtil)
    (maintainGoal {@i TELL ?truth ?person /motivatedBy ?personAsk})
    (maintainGoal {@i TELL ?lie ?person /motivatedBy ?personAsk})).

# Event-form is either sentence (/st) or clause (/cl).
# Arguments are always clauses no matter what.
# Questions are functions, so we don't need an event-flag for them
# Imperatives like Go!, Look! etc., and common phrases (like "hi") are just their words:

# Tell = imperatives
# John said "Go" to me
{/st john tell go @me}
# John shouted "Look out!"
{/st /exclaim john tell lookOut @me}

# Say = statements
{/st john say hi @me}
{/st john say {/cl sam hit sue}}

# Ask = questions (target is always a function)
{/st john ask (what {/cl @you name ?}) @me}
{/st john ask (max {/cl @you observe john}.end) @me}
{/st john ask (own (o [n John Smith] [date Apr-18-1897]) [n 14 Victoria Ave] [/during March-1901] /qs)}
# John asks "Do you want to marry me?"
{/st john ask (maintainProposal {@you marry @me} /qs)}

# Make say/read forgettable, so that once the intake rule is done, we don't pollute the mind
{/st john say {/cl @you name Foo} @me} -> {/st @my name Foo /src [john listen]}
{/st @i read {/cl @you name Foo} ?doc} -> {/st @my name Foo /src [?doc reading]}

# We can handle this with very general rules, including a probability calculation:
{?person say ?clause @me}
    ->
(e /st ?clause /p (p ?clause from ?person)).

{@i read ?clause ?doc}
    ->
(e /st ?clause /p (p ?clause from ?doc)).


# Multiple complete sentences are expressed using a list.
# NOTE that each sentence retains /st rank because they are nested in a list -- not a function or event.
# John walked home. Sam hit Sally.
[{/st john walk home} {/st sam hit sally}]

# Conjugated sentences are given in a list, connected by 'and':
# John walked home and Sam hit Sally.
[{/st john walk home} and {/st sam hit sally}]
# I am either rich or poor.
[{/st @i wealth rich} or {/st @i wealth poor}]
    

# John is wearing a black cotton frock, an orange plaid vest, a white linen shirt, black wool pants, and a black tophat.

{john wear (o frock [color black] [material cotton])}
{john wear (o vest [color orange] [pattern plaid])}
{john wear (o shirt [color white] [material linen])}
{john wear (o pants [color black] [material wool])}
{john wear (o topHat [color black])}


#    giving the following as a condition:
(/known ; human ; name 'John Smith'): ?johnSmith
#    is 'parser sugar' for:
{?johnSmith name 'John Smith'}
{?johnSmith cat human}
#    only because the /known flag was given.  
#    Otherwise it would turn into:
(o human ; name 'John Smith'): ?johnSmith


 Natural -> Detailed language transformations:
 John walked home ->
 John walked to home of John ->
 Detailed language -> MC:
 {(n John):?1 /past walkTo (o [phys bldg] ; home /of ?1):?2}

 MC -> Detailed language:
 {(n John):?1 /past walkTo (o [phys bldg] ; home /of ?1)} ->
      (n John):?1 -> 'John' (aka ?1)
      /past walkTo  -> walked to
      (o [phys bldg] ; home /of ?1) ->
          'home of ?1' ->
          'home of John' (via lookup)
 =>
 John walked to home of John
 Detailed to natural:
 John walked home


{?person goal {?org workplace (o /descr /a ; building ; owner @nothing)}}


{@self job ?clerkJob}
{?clerkJob profName clerk}
{?job org ?org}
{?org workplace @something:?workplace}
(o /a /known stack ; owner ?org ; inList ^?workplace): ?displacedStack
    ->
# Later on, we can add spaces where stacks can go, and place them in the workplace.
# Then, the NPC can put the stacks into such empty spaces (not already filled with stacks)
# For efficiency reasons, such spaces are invisible to all NPCs, and only NPCs with the
# right job will know about them: part of starting that job will be learning about them.
#
# For now, we simply put the stacks in the middle of the workplace
goal({@self put ?displacedStack ?workplace}).

*/
