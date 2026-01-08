# Configuration File

## config.ini

The config.ini file should be placed in the same directory as the LOJ1897.exe executable.

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

**player_model**: The path to the player character model (.wiscene), relative to project_path. Optional (for future use).

**npc_model**: The path to the NPC character model (.wiscene), relative to project_path. Optional (for future use).

### Example

```
project_path = C:\Users\YourName\Documents\LOJ1897
theme_music = SharedContent\Music\Dataspel Tema 4_4 Em 144-v2.mp3
level = SharedContent\Environment\FieldOfGrass.wiscene
```

### Notes

- Paths can use either forward slashes or backslashes
- Values can be enclosed in quotes which will be automatically removed
- Spaces around the equals sign are trimmed automatically
- If the config.ini file is not found or the project_path is empty the game controls will be disabled
- The configuration is automatically saved when the application exits
- When Play Game is pressed:
  - The main menu will be hidden
  - The menu music will stop playing
  - The level scene will be loaded
  - The camera will be positioned at the center of the scene
