# Lua Video API

This document describes the Lua video playback API available to skins in USC.

## Availability

- Requires building USC with `ENABLE_VIDEO=ON`.
- Video decoding uses FFmpeg (configured at build time).

## Constructors

### `gfx.LoadVideo(path)`

Loads a video from a filesystem path and returns a `VideoPlayer` userdata object.

- `path` can be absolute or relative.
- Relative paths are resolved by USC through `Path::Absolute(...)`.
- On failure, the function returns no value (Lua `nil`) and logs an error.

### `gfx.LoadSkinVideo(path)`

Loads a video from the current skin folder and returns a `VideoPlayer`.

- Internally resolves to:
  - `skins/<current-skin>/videos/<path>`
- On failure, the function returns no value (Lua `nil`) and logs an error.

## Quick Start

```lua
local video = gfx.LoadSkinVideo("intro.mp4")
if video then
    video:SetLoop(true)
    video:Play()
end

function render(deltaTime)
    if not video then
        return
    end

    video:Tick(deltaTime)
    local img = video:GetImage()
    if img ~= 0 then
        gfx.ImageRect(0, 0, 1920, 1080, img, 1.0, 0.0)
    end
end
```

## `VideoPlayer` Methods

### `video:Play()`

Starts playback.

- If playback ended and loop is enabled, this seeks to `0.0` first.

### `video:Pause()`

Pauses playback.

### `video:Seek(seconds)`

Seeks to a position in seconds.

- Clamped to `[0, duration]` when duration is known.
- Clears pending decoded frame state so presentation restarts cleanly.

### `video:SetLoop(loop)`

Enables/disables looping.

- `loop` is a boolean.

### `video:SetVolume(volume)`

Sets player volume (`0.0` to `1.0`).

- In the current video-only milestone, video audio output is deferred.
- This value is stored but not yet applied to audible output.

### `video:Tick(deltaTime)`

Advances playback and uploads due frames to the NanoVG image.

- Call once per frame while rendering.
- No frame upload occurs when paused.

### `video:GetImage() -> imageHandle`

Returns the NanoVG image handle for drawing with `gfx.ImageRect(...)`.

### `video:GetPosition() -> seconds`

Returns the current presented video PTS in seconds.

### `video:GetDuration() -> seconds`

Returns the total duration in seconds (or `0` when unknown).

### `video:IsPlaying() -> bool`

Returns whether the player is currently playing.

### `video:HasEnded() -> bool`

Returns whether playback reached end-of-stream (non-looping case).

### `video:GetSize() -> width, height`

Returns decoded video dimensions in pixels.

### `video:Dispose()`

Stops decoding and releases underlying resources early.

- Safe to call manually.
- Also handled by userdata garbage collection (`__gc`).

## Lifecycle and Cleanup

- USC registers a GUI disposal handler for each Lua state.
- On script reload/dispose, active `VideoPlayer` instances for that state are closed.
- This prevents leaked decode threads and leaked NanoVG textures across reloads.

## Notes

- This milestone is video-first: playback is currently silent by design.
- Supported container/codec combinations depend on the FFmpeg build on your system.
- For smooth playback, always call `Tick(deltaTime)` from your frame/update loop.
