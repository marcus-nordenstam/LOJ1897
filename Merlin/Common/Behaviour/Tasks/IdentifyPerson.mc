

# --------------------------------------------------
# Identifying a person by the kind of job they have:
# --------------------------------------------------

rule 
# If I'm trying to identify a person
{@self identify ?person}
# and this person has a specific KIND of job
{?person job [k job]:?job}
# at a specific KIND of company
{?job at ?org}
{?org isa [k org]:?orgKind}
# and I know of a company of that kind, and where it is
{[k org]:?knownOrg isa ?orgKind}
{?knownOrg workplace ?workplace}
# I belive that the known company exists (it's not hypothetical or imagined)
(real ?knownOrg)
# ...but I am not there
(in @self /not ?workplace /cont)
    ->
# then go there.
(maintainProposal {@self go ?workplace}).

rule 
# If I'm trying to identify a person
{@self identify ?person}
# and this person has a specific kind of job
{?person job ?job}
{?job isa [k job]:?jobKind}
# at a specific kind of company
{?job at ?org}
{?org isa [k org]:?orgKind}
# and I know of a company of that kind
{[k org]:?knownOrg isa ?orgKind}
{?knownOrg name ?orgName}
# and I am in its workplace
{?knownOrg workplace ?workplace}
(in @self ?workplace /cont)
# and I see a real person in there whose job I don't know
(o /per /notI [k human] (none {@o job ?}) (in @o ?workplace /cont)): ?realPerson
    ->
# then I want to know if they have that specific kind of job at the company
(qs (real /truth ?jobKind {?realPerson job @o} {@o at (o /known ?orgName)})): ?qsDoYouHaveJobAtOrg
(maintainGoal {@self knowAnswer ?qsDoYouHaveJobAtOrg} /relUtil 1).


/*
# person is a lawyer  vs  person's lawyer-job ?
(o {?person job @o} [k lawyer])

# person is a laywer
(o {/st ?person job @o} [k lawyer])
# person's lawyer-job
(o {?person job @o} [k lawyer])

# ?person is a lawyer at Jersey Law Co
{?person job (o [k lawyer] {@o at (o [n Jersey Law Co])})}


# Is ?person a lawyer?
# NOTE (o...) will infer /known if given as a /qs
(prob {?person job (o [k job lawyer])})


# Authoring & NL-printing form                      firing form
(fire) {?person job (o [k job lawyer])}         -> (o {?person job @o} [k job lawyer])
(fire) {(o /every [k human]) job (o /every [k job lawyer])}   -> (o [k human] {@o job (o {? job @o} [k job lawyer])})


# There is a human lawyer
(e {(o [k human]) job (o [k job lawyer])})  -> 
    ?firedSub = (o /every {@o job ?} [k human])
    ?firedLab = job
    ?firedTar = (o {?firedSub job @o} [k job lawyer])
    compose {?firedSub ?firedLab ?firedTar}

# Is there a human lawyer?
(prob {(o [k human]) job (o [k job lawyer])})


# When firing an event input fields:
#   If a field has (o-func):
#       Create a new uncommitted (o-func phrase)
#       Stash event symbols
#       Replace event field symbol with @o
#       Replace any other event-field with (o) with ?
#       Add the munged event as the first arg
#       Add the remaining args from the o-func
#       Fire the uncommitted (o-func)
#       discard the uncommitted o-func
#       Copy back the event symbols
# If there are more than 1 O-funcs in the event, we must use /every on the uncommitted o-funcs, except for the last one.

*/