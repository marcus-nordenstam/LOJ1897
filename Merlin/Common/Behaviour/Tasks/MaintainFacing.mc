
# Bear in mind that facing is NOT exclusive

rule maintain-facing-attention
{@self keepFacing ?thing}
    ->
(maintainAttention ?thing).

rule maintain-facing-belief
{@self keepFacing ?thing}
#(print [@self facing ?thing] /cont)
(isFacing ?thing 0.9 /cont) = ?prob
    ->
(maintainBelief {@self facing ?thing /p ?prob}).

rule maintain-facing-turnTo-proposal
{@self keepFacing ?thing}
{@self /not facing ?thing}
{?thing obb ?obb}
    ->
(maintainProposal {@self TURN_TO ?obb}).

rule maintain-facing-locate-proposal
{@self keepFacing ?thing}
{@self facing ?thing /p @unknown%}
    ->
(maintainProposal {@self locate ?thing}).