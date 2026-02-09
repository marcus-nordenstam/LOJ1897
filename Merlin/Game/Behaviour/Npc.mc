
# NPCs have common sense
rule ->
(import "CommonReasoning")
(import "CommonActions")
(import "CommonTasks").


# Startup behaviour

rule ->
(beginBelief {@self spouse @nothing})
(beginBelief {@self fiancee @nothing})
(beginProposal {@self startup}).


rule startup-female
{@self startup}: ?startup
{@self gender female}
    ->
(setOutcome ?startup /succ).


rule startup-male-spawnRing
{@self startup}
{@self gender male}
    ->
(beginProposal {@self SPAWN ["prop" [k engagementRing] @self (floats 0.2 1 1) (floats 0 0 0 1)]}).

rule startup-male-takeRing
{@self startup}
{@self gender male}
{[k engagementRing]:?ring obb ?}
# I will only take my ring
{?ring owner @self}
# No hands except mine control the ring
#{@self hand [k leftHand]:?lhand}
#{@self hand [k rightHand]:?rhand}
#(none {[!?lhand & !?rhand] control ?ring})
    ->
(maintainProposal {@self get ?ring}).

rule startup-male-succ
{@self startup}: ?startup
{@self gender male}
{@self /succ get [k engagementRing]:?ring}
{?ring owner @self}
    ->
(setOutcome ?startup /succ).

rule startup-male-fail /breakOnFire
{@self startup}
{@self gender male}
{[k engagementRing]:?ring obb ?}
{@self /fail get ?ring}
    ->
(maintainProposal {@self get ?ring}).



/*
{@self romanticProspect ?prospect}
{@self know ?prospect}
(qs (prob {?prospect goal.task {?prospect marry @self}})): ?doYouWantToMarryMe
(none {@self /succ ask ?doYouWantToMarryMe ?prospect})
    ->
(maintainGoal {@self ASK ?doYouWantToMarryMe ?prospect}).
*/

# (qs (real {?prospect goal @o} {@o act {?prospect marry @self}})): ?doYouWantToMarryMe
# {john sister.friend.name joe}

# {?obj lab[0]..lab[N] ?target}:
#
# every {?obj lab[0] ?} -> [mobjects]
# N++

# foreach object in [mobjects]
#   any: out = any {object lab[1] ?}
#        if out != @fail return out
#   every: every {object lab[1] ?}

# (any|every {object lab[N] ?}):
#   eventRoles = object.lab[N]
#   if N==last label:
#       foreach eventRole in [eventRoles]
#           event = event(eventRole)
#           if event.target matches ?target
#               any: return top-level eventRole keyed off lab[0]
#               every: addItem(list, top-level eventRole keyed off lab[0])
#   else:
#       foreach eventRole in [beliefRoles]
#           event = event(eventRole)
#           nextObject = event.target
#           any: return (any {nextObject lab[N+1] ?}):
#           every: addItem(list, every {nextObject lab[N+1] ?})
#   any: return @fail
#   every: return list

/*

{@self spouse @nothing}
{?prospect /succ TELL (msg @true) @self}: ?prospectToldMe
{?task task {?prospect marry @self}}
{?prospect goal ?task /sources ~?prospectToldMe}
    ->
(maintainProposal {@self marry ?prospect}).


{@self goal ?task}
{?task task {@self marry ?prospect}}
    ->
(e {?task participants [@self ?prospect]}).


{@self goal ?otask}
{?otask participants ?participants}
{?otask task {@self ? ?}:?selfTask}
#(every {?participants goal.task ?selfTask}): ?willingParticipants
(every {?participants goal ?otask}): ?willingParticipants
(eq (count ?participants) (count ?willingParticipants))
    ->
(maintainProposal ?selfTask).

*/
