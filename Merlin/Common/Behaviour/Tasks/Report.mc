
/*
rule 
{@self report ?infos ?audience}: ?reportInfos
(unknowns {@self /succ TELL @litem ?audience /causes ~?reportInfos} ?infos): ?unreportedInfos
    ->
(if (empty ?unreportedInfos)
    (setOutcome ?reportInfos /succ)
# else
    (maintainGoal {@self TELL @litem ?audience} ?unreportedInfos /relUtil (mul @lindex 0.1))).
*/