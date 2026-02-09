

ffi = require("ffi")
mx = ffi.load(cwd .. "/merlin")

ffi.cdef[[

    // This must match Constants.h
    enum { kAttrCapacityPerSystem = 64 };

    // These must match CInterface.h
    enum {
        kVarBindingsCapacity = 64,
        kSymbolArrayCapacity = 256,
        kStringCapacity = 512
    };

    typedef struct {
        char *attrNames[kAttrCapacityPerSystem];
        int counts[kAttrCapacityPerSystem];
        int size;
    } CountsTable;

    typedef struct {
        char *valueNames[kAttrCapacityPerSystem];
        double freqs[kAttrCapacityPerSystem];
        int size;
    } ValueTable;

    typedef struct {
        char *attrNames[kAttrCapacityPerSystem];
        ValueTable valueTables[kAttrCapacityPerSystem];
        int counts[kAttrCapacityPerSystem];
        int size;
    } AttrTable;

    typedef struct {
        const char *varNames[kVarBindingsCapacity];
        uint64_t values[kVarBindingsCapacity];
        int numBindings;
    } MerlinVarBindings;

    typedef struct {
        uint64_t buffer[kSymbolArrayCapacity]; // raw symbol data
        int size;
    }  MerlinSymbolArray;

    typedef struct  {
        uint64_t acts[kSymbolArrayCapacity];            // raw act-symbol data
        int numCauses[kSymbolArrayCapacity];            // the number of causes / act
        uint64_t causes[kSymbolArrayCapacity][64];      // causes
        uint64_t outcompetedBy[kSymbolArrayCapacity];   // the act that prevented this act from being selected
        float util[kSymbolArrayCapacity];               // act-utilities
        int size;
    } MerlinActArray;

    typedef struct {
        char buffer[kStringCapacity];
    } MerlinString;

    typedef struct {
        float floats[3];
    } MerlinFloat3;

    typedef struct {
        float floats[4];
    } MerlinQuat;

    typedef struct {
        float floats[10];
    } MerlinObb;

    typedef struct  {
        MerlinFloat3 *minCoords;
        MerlinFloat3 *maxCoords;
        int size;
    } MerlinSectorArray;

    typedef uint64_t (*MerlinCallback)(uint64_t, uint64_t, uint64_t, uint64_t);
    typedef void (*SoundCallback)(uint64_t);

    void registerCallback(const char *funcName, MerlinCallback func);
    void registerSoundCallback(SoundCallback func);

    void start(const char *merlinRootPath, const char *baseModulePath);
    void stop();
    void setRandomSeed(uint32_t seed);
    void toggleChannel(const char *channel, const char *onOrOff);
    void startTrace(uint64_t rawSymbol); 
    void stopTrace(uint64_t rawSymbol);
    void traceEvent(const char *patternStr);

    MerlinSymbolArray mentalObjectsOfKind(const char *kindTxt);
    MerlinSymbolArray every(const char *crt);

    uint64_t anyPresentTensePossessiveBeliefTarget(uint64_t rawSubject, uint64_t rawLabel);
    MerlinSymbolArray everyPresentTensePossessiveBeliefTargets(uint64_t rawSubject, uint64_t rawLabel);

    MerlinSymbolArray ls(const char *pathStr, const char *extStr);
    void import(const char *sysName);

    void makeCountsTable(CountsTable *outTable, const char *path);
    void makeValueTable(ValueTable *outTable, const char *path);
    void updateAttrTable(AttrTable *outTable, const char *attrNameStr, ValueTable *valueTable, CountsTable *countsTable);
    int numAttrTableValues(AttrTable *outTable, const char *attrNameStr);
    char *sampleAttrTable(AttrTable *outTable, const char *attrNameStr);

    void makeFlatWorld(const float *worldMin, const float *worldMax, float sectorRadius, float groundElevation);
    void makeWorldFromTerrain(const char *terrainFileName, float sectorRadius);
    float terrainElevation(float wx, float wy);
    void loadSceneLayout(const char *layoutPathName, const char **rawKindToSystemTable, int tableSize);

    void setNpcLocomotionMode(const char *mode);

    uint64_t makeRootEntityNow(const char *systemName, const char *kindStr, MerlinObb obb);
    uint64_t makeSubEntityNow(const char *systemName, const char *kindStr, MerlinObb obb, uint64_t rawParentAbsObject);

    uint64_t makeRootEntityAtTime(const char *systemName, const char *kindStr, uint64_t creationSeconds, MerlinObb obb);
    uint64_t makeSubEntityAtTime(const char *systemName, const char *kindStr, uint64_t creationSeconds, MerlinObb obb, uint64_t rawParentAbsObject);

    void addSymbol(MerlinSymbolArray *array, uint64_t rawSymbol);
    void removeSymbolByValue(MerlinSymbolArray *array, uint64_t rawSymbol);
    void removeSymbolByIndex(MerlinSymbolArray *array, int index);

    void addString(MerlinSymbolArray *array, const char* str);

    MerlinSymbolArray npcs();

    uint64_t findEntityByName(const char *systemNameStr, uint64_t rawFirstName, uint64_t rawSurname);
    MerlinSymbolArray findEntitiesOfKind(const char *systemNameStr, const char *kindStr);

    uint64_t instantiateEntity(const char *protoSystemName, const char *instSystemName, uint64_t rawKind);

    uint64_t attrSymbol(uint64_t rawEntity, const char *attrName);
    MerlinSymbolArray attrSymbolArray(uint64_t rawEntity, const char *attrName);

    void setSymbolAttr(uint64_t rawEntity, const char *attrName, uint64_t rawSymbol);
    void setSymbolArrayAttr(uint64_t rawEntity, const char *attrName, MerlinSymbolArray *array);
    void addSymbolAttr(uint64_t rawEntity, const char *attrName, uint64_t rawSymbol);
    void setLocalBoundsAttr(uint64_t rawEntity, const char *attrName, MerlinObb inObb);

    void enterMind(uint64_t rawEntity);
    void enterAbsMind();

    void addMotor(const char *actLabelStr, const char *motorStr);
    void removeMotor(const char *actLabelStr, const char *motorStr);

    uint64_t observe(uint64_t rawEntity);
    uint64_t learnAllAbout(uint64_t rawEntity);
    uint64_t evalSymbol(uint64_t rawSymbol);
    uint64_t evalPattern(const char *str, MerlinVarBindings* inBindings);
    uint64_t learn(const char *str, MerlinVarBindings* inBindings);
    uint64_t believe(const char *str, MerlinVarBindings* inBindings);
    void readMsg(uint64_t rawMsgSymbol);

    uint64_t imagine(const char *str);

    uint64_t hstrSymbol(const char *str);
    uint64_t floatSymbol(float flt);

    uint64_t absObjectFromSymbol(uint64_t rawSymbol);
    uint64_t absObjectFromEntitySystem(const char *entSysName, int entityIndex);
    
    uint64_t list(MerlinSymbolArray symbols);

    void logDebug(const char *text);

    MerlinString toString(uint64_t rawSymbol);
    MerlinString toNlString(uint64_t rawSymbol);

    void allPerceive();
    void allSleep();
    void sim(int cycles);

    MerlinObb composeBounds(MerlinFloat3 pos, MerlinFloat3 scale, MerlinQuat quat);
    MerlinObb mentalBounds(uint64_t rawSubject, uint64_t rawSymbol);
    MerlinObb worldBounds(uint64_t rawSymbol);
    MerlinFloat3 worldPos(uint64_t rawSymbol);
    void setOccluder(uint64_t rawEntity, int state);

    uint64_t seconds();
    uint64_t secondsSymbol();
    uint64_t makeSeconds(uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute, uint32_t second);
    uint64_t dateSymbol();
    uint32_t cycle();

    void setDate(int year, int month, int day);
    void setTime(uint32_t hour, uint32_t minute, uint32_t second);
    void setDateAndTime(int year, int month, int day, uint32_t hour, uint32_t minute, uint32_t second);

    bool isAStr(uint64_t rawEntity, const char *kindStr);
    bool isASymbol(uint64_t rawEntity, uint64_t rawKindSymbol);
    uint64_t kindSymbol(int kindId);

    uint64_t assembleName(uint64_t rawFirstName, uint64_t rawSurname);
    uint64_t assembleRandomFullName(const char *entSysName, const char *firstNameKindStr, const char *surnameKindStr);
    uint64_t assembleRandomName(const char *entSysName, const char *firstNameKindStr, uint64_t rawSurname);

    const char *nlMsg(uint64_t rawSound);

    MerlinSectorArray worldSectors();
    MerlinSectorArray visibleSectors();

    void dumpSelfKnowledge();
    void allDumpSelfKnowledge();

    MerlinSymbolArray firingRuleNames(uint64_t rawSubject);
    MerlinSymbolArray ongoingSelfStates(uint64_t rawSubject);
    MerlinSymbolArray currentGoals(uint64_t rawSubject);
    MerlinActArray proposedTasks(uint64_t rawSubject);
    MerlinActArray runningTasks(uint64_t rawSubject);
    MerlinActArray proposedActions(uint64_t rawSubject);
    MerlinActArray runningActions(uint64_t rawSubject);
]]

MerlinSymbolArray = ffi.typeof("MerlinSymbolArray")
