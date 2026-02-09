
action ?actorEnt OFFER ?thingEnt ?recipientEnt
    ->
# TODO add additional proximity checks that his hand is close enough to take etc
(intAction): ?maction
# When offering, the gripper holds the item lightly, so it can be taken.
(setAttr ?thingEnt "controlForce" 0)
# Once my hand no longer grips the thing, the offer is done
(attr ?thingEnt "controlledBy"): ?gripperEnt
(perceiveEntity ?gripperEnt): ?gripper
(if (none {@self hand ?gripper})
    [(print [@self DONE OFFERING ?thingEnt]) (setActionOutcome /succ)]).