

rule 
{@self perform ?job}
{?job at ?org}
{?org workplace ?workplace}
(in /not /cont @self ?workplace)
    ->
(maintainProposal {@self go ?workplace}).

