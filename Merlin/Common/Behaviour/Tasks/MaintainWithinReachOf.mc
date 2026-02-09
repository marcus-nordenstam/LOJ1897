
# Bear in mind that withinReachOf is NOT exclusive

rule maintain-withinReachOf-attention
{@self keepInReachOf ?thing}
    ->
(maintainAttention ?thing).

rule maintain-withinReachOf-belief
{@self keepInReachOf ?thing}
(isWithinReachOf ?thing /cont) = ?prob
    ->
(maintainBelief {@self withinReachOf ?thing /p ?prob}).

rule maintain-withinReachOf-go-proposal
{@self keepInReachOf ?thing}
{@self /not withinReachOf ?thing}
    ->
(maintainProposal {@self go ?thing}).

rule maintain-withinReachOf-locate-proposal
{@self keepInReachOf ?thing}
{@self withinReachOf ?thing /p @unknown%}
    ->
(maintainProposal {@self locate ?thing}).