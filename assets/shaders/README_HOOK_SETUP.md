# Shader Hook System - Setup & Configuration

## Project Location
The example `.hook` files in this directory (`assets/shaders/`) are reference and documentation files for the PPSSPP project. 
During development, these files demonstrate the hook syntax and available commands.

## Runtime Location (User's Emulator)
At runtime, `.hook` files must be placed in the **shaders** subdirectory of your PSP directory:

```
{PSP_Directory}/shaders/*.hook
```

Copy example `.hook` files from this directory to your runtime PSP/shaders/ folder if you want to test them.

### Example Paths

**Linux/Mac:**
```
~/.ppsspp/PSP/shaders/my_custom_hook.hook
```

**Windows:**
```
C:\Users\YourName\AppData\Roaming\PPSSPP\PSP\shaders\my_custom_hook.hook
```

**Android:**
```
/sdcard/PSP/shaders/my_custom_hook.hook
```

The exact PSP directory path depends on what you configured during PPSSPP's initial setup.

## Scan Location

The hook system scans only this location:
- `{PSP_Directory}/shaders/` - The shaders subfolder of your user-configured PSP directory

Place all `.hook` files in this directory. No fallback paths.

## Hook File Format

### Header Section
```ini
// ==Hook==
Name = Display Name
Author = Your Name
GameID = *              ; * for all games, or specific game ID
Stage = Fragment        ; Currently only Fragment is supported
Point = AfterLighting   ; See HookPoints below
Enabled = true          ; Set to false to disable without deleting
```

### Code Section
```ini
// ==Code==
ReduceBloom(0.20);
Contrast(1.15);
Saturation(1.35);
```

## Available HookPoints

| HookPoint | Stage | Description |
|-----------|-------|-------------|
| **BeforeLighting** | Fragment | Before lighting calculations (affects vertex color) |
| **BeforeTexture** | Fragment | Before texture sampling |
| **AfterLighting** | Fragment | After lighting pass completes |
| **AfterTexture** | Fragment | After texture sampling completes |
| **BeforeFog** | Fragment | Before fog processing |
| **AfterFog** | Fragment | After fog calculations applied |
| **BeforeBlend** | Fragment | Before framebuffer blending |
| **AfterBlend** | Fragment | After blending completes |
| **BeforeFinalColor** | Fragment | Final output point, before writing to framebuffer |

**Note:** Vertex shader hooks are parsed but not yet executed.

## Debugging

Check the emulator log (usually in `logs/` directory) for messages like:
```
===== Shader Hook System =====
Scanning for .hook files in: /path/to/PSP/shaders/
LOADED [filename.hook]: Hook Name (GameID: *, Stage: Fragment, Point: AfterLighting)
Total: 1 loaded, 0 failed
=============================
```

If your hooks aren't loading, check:
1. File is in the correct `{PSP}/shaders/` directory
2. Syntax is correct (uppercase keywords, semicolons after commands)
3. `Enabled = true` is set in the hook header
4. Check the log for parse errors

## Example: Create a Simple Hook

1. Create file: `/path/to/PSP/shaders/my_brightness.hook`
2. Add content:
   ```ini
   // ==Hook==
   Name = My Brightness Boost
   Author = Me
   GameID = *
   Stage = Fragment
   Point = BeforeFinalColor
   Enabled = true

   // ==Code==
   ReduceBloom(0.50);
   ```
3. Save and restart PPSSPP
4. The hook should be loaded automatically on startup

## Supported Commands

See `all_commands.hook` for the complete list of 26+ supported shader transformation commands.

Each command has recommended HookPoints for best results - placing commands at their recommended points ensures predictable behavior.
