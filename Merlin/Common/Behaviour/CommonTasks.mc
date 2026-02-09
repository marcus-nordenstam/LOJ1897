
rule importMovementTasks 
    ->
(import "Go")
(import "MaintainWithinReachOf")
(import "MaintainFacing")
(import "Explore")
(import "Locate")
(import "GoHome").


rule importResourceTasks 
    ->
(import "Give")
(import "Receive")
(import "Get")
(import "BuyBuilding").


rule importLearningTasks
    ->
(import "IdentifyPerson")
(import "SeekAnswers")
(import "GetAcquaintedWith")
(import "LearnTarget")
(import "LearnProb")
(import "Read")
(import "PerceiveAttr").


rule importSocialTasks
    -> 
(import "Ask")
(import "AnswerQuestion")
(import "HearAnswer")
(import "Tell")
(import "HearTell")
(import "WriteDoc")
(import "Introduce")
(import "Conversation")
(import "Socialize").


rule importStackTasks
    ->
(import "StackBrowse")
(import "StackGet")
(import "StackPut")
(import "StackRead")
(import "MakeDoneStack").


rule importFamilyTasks
    ->
(import "Engagement")
(import "Marriage")
(import "Parenting").


rule importWorkTasks
    ->
(import "Work")
(import "FoundOrg")
(import "HouseAgentClerkJob").


rule importLifeCycleTasks
    ->
(import "GiveBirth")
(import "Die").
