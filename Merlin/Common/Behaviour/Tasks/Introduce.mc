
# :ST
#   {@self brother (o ?PNAME)}
#   "I have a brother named ?PNAME"

#   (this ?ENT [k car] {o1 possess @o})
#   "This is my car"

#   (this ?ENT ?PNAME {@self brother @o})
#   "This is my brother ?PNAME"

#   (this ?ENT ?PNAME {@self brother @o}) [casual intro]
#   "Let me introduce my brother ?PNAME"

#   (this ?ENT ?PNAME {@self brother @o}) [formal intro]
#   "Allow me introduce my brother ?PNAME"

# :NP
#   (o ?PNAME {@self brother @o})
#       "my brother ?PNAME"

/*
# This hammer saved my life
{(this ?hammer [k hammer]) save (of life @self) /past}

# I have a car
{@self possess (o [k car])}

# This is my car (e.g. I have this car)
{@self possess (this ?ENT [k car])}

# This is my brother (e.g. I have this brother)
{@self brother (this ?ENT)}

# This is my brother John (e.g. I have this brother John)
{@self brother (this ?ENT [n John])}

# This is John's brother (e.g. John has this brother)
{?john brother (this ?ENT)}

# Let me introduce my brother ?PNAME
{@self brother (this ?ENT ?PNAME)} [casual intro]

# Allow me to introduce my brother ?PNAME
{@self brother (this ?ENT ?PNAME)} [formal intro]
*/

# Family-based intros:
#
# mother intros siblings
# each parent:
#   intro child to your parents
#   intro child to your siblings
# if your sibling intros their child, then:
#   intro that child to your children (as cousins)




# Getting acquianted (in general)
#   up to each NPC to decide
#       reasons (one per rule)
#           to classify/determine romantic prospect
#               you must be single and ageGroup 3+
#               not part of family
#               opposite gender
#               ageGroup 3+
#           to become friends
#               you must be same gender
#               you must be same age-group
#           out of curiosity (could be random or whatever)
#   here we have personality for the first time:
#       extrovert - you will initiate getting to know regardless if you've never spoken to them
#       introvert - you will only initiate if the person has already talked to you


# PHYSICAL introductions are behaviours whose purpose is to establish a direct relationship
# between mental objects and their environmental entities.
#
# Unlike regular perception, where the person perceiving makes that connection by themselves,
# a physical introduction involves a 'speaker' *communicating* that connection to a recipient.
#
# This means that all parties involved must be physically present so that the mental/physical
# links can be made.  This makes physical introductions fundamentally different from other
# communicative behaviours where the speaker is talking about an object which may not be 
# physically present, and where indirect object references must be used.

rule 
{@self introduceSiblings ?child1 ?child2}
{?child1 name ?child1Name}
{?child2 name ?child2Name}
{?child1 gender ?child1Gender}
{?child2 gender ?child2Gender}
# In order to perform this PHYSICAL introduction, we have to check that all parties can directly
# refence each other (e.g. that they have all experienced each other and that all of them 
# still exist in the physical environment)
(canDirectRef /cont @self ?child1 ?child2)
    ->
# tell child1 "this is your brother|sister ?child2Name"
(call siblingRel ?child2Gender): ?child2Rel
(msg {?child1 ?child2Rel (this ?child2 ?child2Name)}): ?msgChild2IsYourSibling
(maintainGoal {@self TELL ?msgChild2IsYourSibling ?child1})
# tell child2 "this is your brother|sister ?child1Name"
(call siblingRel ?child1Gender): ?child1Rel
(msg {?child2 ?child1Rel (this ?child1 ?child1Name)}): ?msgChild1IsYourSibling
(maintainGoal {@self TELL ?msgChild1IsYourSibling ?child2}).

rule 
{@self /ever introduceSiblings ?person1 ?person2 /noOut}: ?intro
{@self /succ TELL ? ?person1 /causes ~?intro}
{@self /succ TELL ? ?person2 /causes ~?intro}
    ->
(setOutcome ?intro /succ).

# Tell/ask no longer make @msg
# Instead msg state makes @msg, which is used to decorate msg-objects
# Message is now a kind of object
# msg function:
#   Create a msg-object, and decorate is with the msg-event, whose target = arg1

# msg function reifies the message as a msg-object 
# and decorates it using the current phrasing, formality, urgency, emotional state, attitude toward recipient,
# applying any given overrides as well
#(msg {@self home (this ?home)} welcoming) ->

#create a message object with the following events:
#(o {@o msg {@self home (this ?home)} [@i @self @you ?recipient @this ?home]}
#   {@o delivery [(phrasing)welcome (formality)informal (urgency)unhurried (emotion)calm (attitude)friendly]})


# COMPOSITION   : Message as composed in reasoning rule
# EXTERNALIZED  : Externalized message
# INTERNALIZED  : Internalized message
# NL-PRINTED    : NL printed message
# NL            : Message in NL

# COMPOSITION   : (msg {o1 love o23} /from o1 /to o23) (fire)-> (o {msg {o1 love o23} [@i o1 @you o23]})
# EXTERNALIZED  : (o {msg {E11 love E23} [@i E11 @you E23]})
# INTERNALIZED  : (o {msg {o55 love o1}  [@i o55 @you o1]}) (fire)-> {o55 love o1}
# NL-PRINTED    : {i love you} - pronoun substitution applied only by NL-printing
# NL            : "I love you"

# COMPOSITION   : (msg {(this o55) name [n John]} /from o1 /to o23) -> (o {msg (this o55) name [n John]} [@he o55]})
# EXTERNALIZED  : (o {msg (this E55) name [n John]} [@he E55]})
# INTERNALIZED  : (o {msg (this o47) name [n John]} [@he o47]}) (fire)-> {o47 name [n John]}
# NL-PRINTED    : {(this E55) name John}
# NL            : "This is john" -- (this E55) collapsed to "this" and the entity ref is discarded


# NPC/NPC     : (msg {o23 brother (this o55 [n John])}) -> (o msg {o23 brother (this o55 [n John])}) -> [{o23 brother o55} {o55 name [n John]}]
# NPC->player : {o23 brother (this o55 [n John])} -> (@you=o23) -> {your brother (this [n John])} -> "This is your brother John" 


# NPC->player : {o23 brother (this o55 [n John])} -> (@you=o23) -> {your brother (this [n John]) [intro]} -> "Meet your brother John" 

# NPC->player : {o23 brother (this o55 [n John])} -> (@you=o23) -> {your brother (this [n John]) [formal intro]} -> "Allow me to introduce your brother John" 

# COMPOSITION   : (msg {o55 emotion sad}) -> (o {msg {o55 emotion sad} [@he o55]})
# EXTERNALIZED  : (o {msg E55 emotion sad} [@he E55]})
# INTERNALIZED  : (o {msg o47 emotion sad} [@he o47]}) (fire)-> {o47 emotion sad}
# NL-PRINTED    : {/S (o [n John]) emotion sad /pres}
# NL            : "John is sad"



# If an object can be externalized, then it means the object still exists.
# But if an object is destroyed (or a person buried), its blind data is destroyed (and re-used by something else).
#
# So even though all minds will have their own local objectId for it, they won't be
# able to externalize it as blind-data anymore.  They must then externalize it by name or other descriptors, as
# we would when writing about it.

# Direct object reference - externalizable
#
# An NPC can *directly* reference object O if there is a current blind-data-mapping from O to the blind-data.
# This is always the case if NPC has previously internalized O directly from its blind-data, such as via perception.
#
# However, if O is an entity E which is destroyed, then that mapping is eliminated from all minds that 
# previously internalized it, and after that point, O will no longer be directly referrable by any NPC.
# If the NPC is aware of O/E having been destroyed, they will have 'ended' the object by ending its 'isa' relation.
# This would put the O in past tense.  If the NPC is not aware, they will still believe 'isa' holds, but will nevertheless
# be unable to generate a direct object reference to it.

# Indirect object reference - non-externalizable:
# 
# An indirect object reference uses the (o) function to describe an object via name and/or other descriptors.

# Talking about objects - these are the cases to handle:
#
# If speaker can dir-ref O, and speaker knows that recipient can also dir-ref O:
#   Speaker tells recipient: "*that* O has name N"
#
# Speaker has can dir-ref O, but recipient cannot.
#   Speaker tells recipient: "O is an object of kind K with name N"
