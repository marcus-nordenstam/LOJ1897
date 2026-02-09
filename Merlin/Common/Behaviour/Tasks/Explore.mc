
# We can have two versions of exploring; one for the whole world
# and another that is limited to a given space.

# We first implement exploring the world (boundless exploration)

#rule 
#{@self explore world}
#(o /per [k exteriorSpace] {@o obb} {@self /not /ever in @o}): ?space
#    ->
#(maintainProposal {@self go ?space}).

#rule 
#{@self explore world}
#{@self go ?space}
#(in @self ?space /cont)
#    ->
#(maintainBelief {@self in ?space}).
