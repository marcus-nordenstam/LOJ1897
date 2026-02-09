
bit = require("bit")

-- Time stepping modes: historical, debug, realtime
realtime   = 1
debug      = 2
historical = 3
_G.timeStepMode = historical
_G.debugDt = 5 -- seconds
_G.historicalDt = 480 -- seconds
stepSim = true
frameSim = false

-- Camera position (used by Merlin spatial queries)
camX, camY = 9000, 13000
zoom = 24

-- Selected NPC tracking
selectedNpc = nil

-- Frame delta
frameDt = 0.1

-- Sound bubbles (for Merlin sound system)
soundBubbles = {}
