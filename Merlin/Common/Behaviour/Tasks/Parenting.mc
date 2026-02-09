
# If you are married, you'll want kids
rule growFamily
{@self gender male}
{@self spouse @something:?spouse}
(lt (count (every {@self child})) 3)
    ->
(maintainProposal /cont {@self HAVE_SEX_WITH ?spouse}).

# We let the mother tell her children about their home
rule tellChildTheirHome
{@self gender female}
{@self home ?home}
{@self child ?child}
{?child ageGroup 0}
(none {@self /succ TELL '(msg {?child home ?home}) ?child})
    ->
(beginGoal {@self TELL (msg {?child home ?home}) ?child})
(fireAndForget).

rule tellChildMyHome
{@self child ?child}
{?child ageGroup 0}
{@self home ?home}
(none {@self /succ TELL '(msg {@self home ?home}) ?child})
    ->
(beginGoal {@self TELL (msg {@self home ?home}) ?child})
(fireAndForget).

# We let the mother introduce the siblings
rule introduceSiblings
{@self gender female}
{@self child ?child1}
{?child1 ageGroup 0}
{@self child !?child1:?child2}
(none {@self /succ introduceSiblings ?child1 ?child2})
    ->
(maintainProposal {@self introduceSiblings ?child1 ?child2}).

# Once the children are old enough, tell them about the theatre (so they can go there later and socialize)
rule tellChildTheatreObb
{@self gender ?gender}
{@self child ?child}
{?child gender ?gender}
{?child ageGroup 1}
{?theatre isa [k building theatre]:?theatreKind}
{?theatre obb ?obb}
    ->
(maintainGoal {@self TELL (msg (o ?theatreKind {@o obb ?obb})) ?child}).

# comeWith task:
#   If someone tells you comeWith, they are the leader and I am the follower
#   leader:
#       while follower is near, go to destination
#       when you are at the destination, tell them "we have arrived" which ends the task.
#   follower:
#       while follower is NOT near, go to leader
#       if the leader tells you "we have arrived", the task is done.

