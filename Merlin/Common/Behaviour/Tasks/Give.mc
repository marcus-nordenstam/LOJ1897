
# --------------------------------------
# Rules for the GIVER in the "give" task
# --------------------------------------

# To give, you must have it
rule give-proposal-getThing
{@self give ?thing ?recipient}
{@self hand ?hand}
{?hand control !?thing}
    ->
(maintainProposal {@self get ?thing}).


# If you have it, you must be close enough to touch the recipient
rule give-goal-withinReachOfRecipient
{@self give ?thing ?recipient}
{@self hand ?hand}
{?hand control ?thing}
    ->
(maintainProposal {@self keepInReachOf ?recipient} /absUtil 1000)
(maintainProposal {@self keepFacing ?recipient} /absUtil 1000).


rule give-proposal-offer
{@self give ?thing ?recipient}
{@self hand ?hand}
{?hand control ?thing}
{@self withinReachOf ?recipient}
{@self facing ?recipient}
(real ?thing)
    ->
(maintainProposal {@self OFFER ?thing ?recipient}).


# ------------------------------------------
# Rules for the RECIPIENT in the "give" task
# ------------------------------------------

rule goal-possess-receive-proposal
# If I want to possess a thing
{@self goal {@self possess ?thing}}
# and someone is offering me that thing
{?giver OFFER ?thing @self}: ?offer
    ->
# then get it
(maintainProposal {@self receive ?thing ?giver}): ?proposal
(addCause ?proposal ?offer). # the offer is not a self-action, so it won't be automatically added as a cause


/*
# If I want a hypothetical document of a specific kind
rule 
{@self goal {@self possess [k document]:?doc}}
# and someone is offering me that kind of document
{?giver OFFER ?offeredDoc @self}
{?offeredDoc isa [k document]:?specificKind}
# which I have never read
(none {@self /succ read ?offeredDoc})
(irrealis ?doc)
    ->
# then read it to see if it is the actual document I want
# (the read task will cause me to take it before reading)
(maintainProposal {@self read ?offeredDoc}).
*/

# ------------------------------------------
# Giving outcomes
# ------------------------------------------

# From the giver's pov
rule give-outcome-giver
{@self /ever give ?thing ?recipient}: ?give
{@self /succ OFFER ?thing ?recipient /causes ~?give}: ?offer
#{?recipient /succ TAKE ?thing /causes ~?offer}
    ->
(setOutcome ?give /succ)
(fireAndForget).

# From the observer's pov
rule give-outcome-observer
{!@self:?giver /succ OFFER ?thing ?recipient}: ?offer
{?recipient /succ TAKE ?thing /causes ~?offer}
    ->
(beginBelief {?giver give ?thing ?recipient /momentary}): ?give
(setOutcome ?give /succ)
(fireAndForget).


/*
# If you have it, then strive to be face-to-face with the recipient
rule 
{@self give ?thing ?recipient}
{@self control ?thing}
    ->
{@self goal {.. maintain {.. faceToFace at ?recipient}} +1000}.

# If you are giving to a player, then you must be face-to-face with them (so they can see what you are doing)
rule 
{@self give ?thing ?player}
{@self control ?thing}
{?player role player}
{@self faceToFace at ?player}
    ->
{propose() {@self OFFER ?thing ?player}}.

# If you are giving to an NPC, then you just have to be in reaching distance
rule 
{@self give ?thing ?npc}
{@self control ?thing}
{?npc role npc}
{@self at ?npc}
    ->
{propose() {@self OFFER ?thing ?npc}}.
*/

