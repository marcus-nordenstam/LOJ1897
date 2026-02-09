
# NPCs have common sense
rule ->
(import "CommonReasoning")
(import "CommonActions")
(import "CommonTasks").


# Startup behaviour

rule go-to-waypoint
{?waypoint isa [k waypoint]}
{?waypoint obb ?obb}
    ->
(maintainProposal {@self go ?obb}).

