
rule 
{@self seekAnswers ?questions}: ?seekAnswers
(unknowns ?questions): ?unknowns # get a list of questions to which I still have no answer
    ->
#(print [SEEKANSWERS @self must learn (count ?unknowns) unknowns])
#(print (nl ?unknowns))
(if (empty ?unknowns)
    [ (setOutcome ?seekAnswers /succ)   (print [TASK DONE (nl ?seekAnswers)]) ] 
# else
    (maintainGoals ?unknowns {@self knowAnswer @litem} /relUtil (mul @lindex 0.1))).