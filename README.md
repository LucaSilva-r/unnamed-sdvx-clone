# USC Extended ![language: C/C++](https://img.shields.io/badge/language-C%2FC%2B%2B-green.svg) [![Build](https://github.com/LucaSilva-r/unnamed-sdvx-clone/workflows/Build/badge.svg)](https://github.com/LucaSilva-r/unnamed-sdvx-clone/actions)

A fork of [Unnamed SDVX Clone](https://github.com/Drewol/unnamed-sdvx-clone) that adds **video playback** and **Live2D model** support to the skinning system.

### What's new in this fork

- **Video playback in skins** — Load and play video files (MP4, WMV, etc.) from Lua scripts using the `gfx.LoadVideo` / `gfx.LoadSkinVideo` API. Videos render to a texture that can be drawn with `gfx.ImageRect`.
- **Live2D Cubism model support** — Load and render Live2D models (`.model3.json`) from Lua scripts using the `gfx.LoadLive2DModel` / `gfx.LoadSkinLive2DModel` API. Models support motions, expressions, physics, eye blink, and breath animations.

### [**Download latest build**](https://github.com/LucaSilva-r/unnamed-sdvx-clone/releases)

### [**FAQ**](https://github.com/Drewol/unnamed-sdvx-clone/wiki/F.-A.-Q.)

#### [**Skinning Documentation**](https://unnamed-sdvx-clone.readthedocs.io/en/latest/index.html)

---

## Skin API: Video

Load a video with `gfx.LoadVideo(path)` or `gfx.LoadSkinVideo(path)` (prepends `skins/[skin]/videos/`).

```lua
local video = gfx.LoadSkinVideo("background.mp4")
video:SetLoop(true)
video:Play()

function render(deltaTime)
    video:Tick(deltaTime)
    local img = video:GetImage()
    if img then
        gfx.ImageRect(0, 0, screenW, screenH, img, 1, 0)
    end
end
```

**Methods:**

| Method | Description |
|---|---|
| `video:Play()` | Start playback |
| `video:Pause()` | Pause playback |
| `video:Seek(seconds)` | Seek to position |
| `video:SetLoop(bool)` | Enable/disable looping |
| `video:SetVolume(volume)` | Set audio volume (0.0 - 1.0) |
| `video:Tick(deltaTime)` | Advance playback (call each frame) |
| `video:GetImage()` | Get NanoVG image handle for `gfx.ImageRect` |
| `video:GetPosition()` | Get current position in seconds |
| `video:GetDuration()` | Get total duration in seconds |
| `video:IsPlaying()` | Check if playing |
| `video:HasEnded()` | Check if playback ended |
| `video:GetSize()` | Get video dimensions (returns width, height) |
| `video:Dispose()` | Free resources |

## Skin API: Live2D

Load a Live2D model with `gfx.LoadLive2DModel(path)` or `gfx.LoadSkinLive2DModel(path)` (prepends `skins/[skin]/live2d/`). The path should point to a `.model3.json` file. Place the full model folder (moc3, textures, motions, etc.) alongside it.

```lua
local model = gfx.LoadSkinLive2DModel("character/character.model3.json")
model:SetSize(2048, 2048) -- render resolution

function render(deltaTime)
    model:Update(deltaTime)
    local img = model:GetImage()
    if img then
        gfx.ImageRect(x, y, size, size, img, 1, 0)
    end
end
```

**Methods:**

| Method | Description |
|---|---|
| `model:SetSize(w, h)` | Set render resolution (default 512x512, use 2048x2048 for 4K) |
| `model:Update(deltaTime)` | Advance animation/physics and render to FBO |
| `model:GetImage()` | Get NanoVG image handle for `gfx.ImageRect` |
| `model:GetParameterNames()` | Get table of parameter names |
| `model:SetParameter(name, value)` | Set a model parameter by name |
| `model:PlayMotion(group, index [, priority])` | Play a motion (priority defaults to 2) |
| `model:SetExpression(name)` | Set a facial expression |
| `model:SetPhysicsEnabled(bool)` | Enable/disable physics simulation |
| `model:Dispose()` | Free model resources |

> **Note:** Live2D support requires building with `ENABLE_LIVE2D=ON` and the [Cubism SDK Core](https://www.live2d.com/en/sdk/about/) placed in `third_party/CubismSdkCore/`. Published builds include Live2D support.

---

## Current features:
- Completely skinnable GUI
- **Video playback in skins** (MP4, WMV, etc. via FFmpeg)
- **Live2D Cubism model rendering in skins**
- OGG/MP3 Audio streaming (with preloading for gameplay performance)
- Uses KShoot charts (`*.ksh`) (1.6 supported)
- Functional gameplay and scoring
- Saving of local scores
- Autoplay
- Basic controller support
- Changeable settings and key mapping
- Supports new sound FX method (real-time sound FX) and old sound FX method (separate NOFX & sound effected music files)
- Song database cache for near-instant game startup (sqlite3)
- Song database searching
- Linux/Windows/macOS support
- Song select UI/Controls to change HiSpeed and other game settings

If something breaks in the song database, delete "maps.db". **Please note this will also wipe saved scores.**

## Controls
### Default bindings (Customizable):
- Start: \[1\]
- BTN (White notes , A/B/C/D): \[D\] \[F\] \[J\] \[K\]
- FX (Yellow notes, L/R): \[C\] \[M\]
- VOL-L (Cyan laser, Move left / right): \[W\] \[E\]
- VOL-R (Magenta laser): \[O\] \[P\]

### Song Select:
- Use the arrow keys or knobs to select a song and difficulty
- Use \[Page Down\]/\[Page Up\] to scroll faster
- \[F2\] to select a random song
- \[F8\] demo mode (continuously autoplay random songs)
- \[F9\] to reload the skin
- \[F11\] to open the the currently selected chart in the editor specified by the `EditorPath` setting
- \[F12\] to open the directory of the currently selected song in your file explorer
- \[Enter\] or \[Start\] to start a song
- \[Ctrl\]+\[Start\] to start song with autoplay
- \[FX-L\] to open up filter select to filter the displayed songs
- \[Start\] when selecting filters to toggle between level and folder filters
- \[FX-L\] + \[FX-R\] to open up game settings (Hard gauge, Random, Mirror, etc.)
- \[TAB\] to open the Search bar on the top to search for songs
- \[BT-B + BT-C\] Add song to collection (such as favourites)

## How to run:
Just run 'usc-game' or 'usc-game_Debug' from within the 'bin' folder.

#### Command line flags (all are optional):
- `-notitle` - Skips the title menu launching the game directly into song select.
- `-mute` - Mutes all audio output
- `-autoplay` - Plays chart automatically, no user input required
- `-autobuttons` Like autoplay, but just for the buttons. You only have to control the lasers
- `-autoskip` - Skips beginning of song to the first chart note
- `-debug` - Used to show relevant debug info in game such as hit timings, and scoring debug info
- `-test` - Runs test scene, for development purposes only
- `-gamedir` - Sets the directory the game loads assets from. If unset, attempts reading from `$XDG_DATA_HOME/unnamed-sdvx-clone`. Finally, uses the executable directory if all else fails.

## How to build:

### Windows:
0. Clone the project using `git` and then run `git submodule update --init --recursive` to download the required submodules.
1. Install [CMake](https://cmake.org/download/)
2. Install [vcpkg](https://github.com/microsoft/vcpkg)
3. Install the packages listed in 'build.windows'
4. Run 'GenerateWin64ProjectFiles.bat' from the root of the project
    * If this fails, try using the `-DCMAKE_TOOLCHAIN_FILE=[VCPKG_ROOT]\scripts\buildsystems\vcpkg.cmake` flag that vcpkg should give you on install
5. Build the generated Visual Studio project 'FX.sln'
6. Run the executable made in the 'bin' folder

To run from Visual Studio, go to Properties for Main > Debugging > Working Directory and set it to '$(OutDir)' or '..\\bin'

### Linux:
0. Clone the project using `git` and then run `git submodule update --init --recursive` to download the required submodules.
1. Install [CMake](https://cmake.org/download/)
2. Check 'build.linux' for libraries to install
3. Run `cmake -DCMAKE_BUILD_TYPE=Release .` and then `make` from the root of the project
4. Run the executable made in the 'bin' folder

### macOS:
0. Clone the project using `git` and then run `git submodule update --init --recursive` to download the required submodules.
1. Install dependencies
	* [Homebrew](https://github.com/Homebrew/brew): `brew install cmake freetype libvorbis sdl2 libpng jpeg-turbo libarchive libiconv`
2. Run `cmake -DCMAKE_BUILD_TYPE=Release .` and then `make` from the root of the project.
3. Run the executable made in the 'bin' folder.

### Building with Live2D support:
1. Download the [Cubism SDK for Native](https://www.live2d.com/en/sdk/download/native/) and place the `Core` folder at `third_party/CubismSdkCore/`
2. Add `-DENABLE_LIVE2D=ON` to your cmake command
3. Place model shader files at `bin/FrameworkShaders/` (symlink to `third_party/CubismNativeFramework/src/Rendering/OpenGL/Shaders/Standard` works)
