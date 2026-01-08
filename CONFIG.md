# Configuration File

## config.ini

The config.ini file should be placed in the same directory as the LOJ1897.exe executable.

**Note:** The config.ini file in the project root is automatically copied to the build directory (Debug/Release/etc.) after every build, so you can edit it in the project root and it will be available in your build output.

### Format

The configuration file uses a simple INI format with key-value pairs:

```
project_path = C:\Users\realm\Dropbox\SGS\LOJ1897
theme_music = SharedContent\Music\Dataspel Tema 4_4 Em 144-v2.mp3
level = SharedContent\Environment\FieldOfGrass.wiscene
player_model = SharedContent\Characters\CC_Characters\Char_Male_Med_Baddie_02\baddie2D.wiscene
npc_model = SharedContent\Characters\CC_Characters\WI_Char_Male_Med_Baddie_01_trimmed.wiscene
```

### Settings

**project_path**: The full path to the project's root directory. This is the base path for all other relative paths. Required.

**theme_music**: The path to the music file to play in the main menu, relative to project_path. Supports MP3, OGG, and WAV formats. Optional.

**level**: The path to the Wicked Engine scene file (.wiscene) to load when Play Game is pressed, relative to project_path. Required for gameplay.

**player_model**: The path to the player character model (.wiscene), relative to project_path. This model will be instantiated at entities with MetadataComponent preset set to "Player". Optional.

**npc_model**: The path to the NPC character model (.wiscene), relative to project_path. This model will be instantiated at entities with MetadataComponent preset set to "NPC". Optional.

**anim_lib**: The path to the animation library file (.wiscene), relative to project_path. This library contains animation data required by the action system. Optional.

**expression_path**: The path to the folder containing expression preset files (.esp), relative to project_path. These expressions are required by the action system for character facial animations. Optional.

### Example

```
project_path = C:\Users\YourName\Documents\LOJ1897
theme_music = SharedContent\Music\Dataspel Tema 4_4 Em 144-v2.mp3
level = SharedContent\Environment\FieldOfGrass.wiscene
player_model = SharedContent\Characters\CC_Characters\Char_Male_Med_Baddie_02\baddie2D.wiscene
npc_model = SharedContent\Characters\CC_Characters\WI_Char_Male_Med_Baddie_01_trimmed.wiscene
anim_lib = SharedContent\Characters\Animation\AnimLib_Trimmed.wiscene
expression_path = SharedContent\Expressions
```

### Notes

- Paths can use either forward slashes or backslashes
- Values can be enclosed in quotes which will be automatically removed
- Spaces around the equals sign are trimmed automatically
- If the config.ini file is not found or the project_path is empty the game controls will be disabled
- The configuration is automatically saved when the application exits
- On startup:
  - The animation library (anim_lib) is loaded and merged into the scene if specified
  - Expression presets (.esp files) are loaded from the expression_path folder if specified
  - These are required by the action system for character animations and facial expressions
- When Play Game is pressed:
  - The main menu will be hidden
  - The menu music will stop playing
  - The level scene will be loaded
  - Player characters will be spawned at entities with MetadataComponent preset "Player"
  - NPC characters will be spawned at entities with MetadataComponent preset "NPC"
  - Characters will have collision detection and physics enabled
  - Mouse is captured and hidden for walkabout-style controls
  - The camera will automatically follow the player character in third-person view
  - Camera collision detection prevents the camera from going through walls
  - WASD keys control movement relative to camera direction
  - Space key makes the player jump
  - Escape key returns to the menu and releases the mouse