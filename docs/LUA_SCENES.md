# 🌙 Lua Scene Development Guide

This document covers everything you need to write, test, and deploy custom LED-matrix
scenes in Lua — no C++ compilation required.

## Table of Contents

- [How It Works](#how-it-works)
- [Scene File Location](#scene-file-location)
- [Script Contract](#script-contract)
  - [Required globals](#required-globals)
  - [Lifecycle functions](#lifecycle-functions)
- [API Reference](#api-reference)
  - [Read-only globals](#read-only-globals)
  - [Drawing functions](#drawing-functions)
  - [Property functions](#property-functions)
- [Property Types](#property-types)
- [Hot-Reload](#hot-reload)
- [Error Handling](#error-handling)
- [Examples](#examples)
- [Pitfalls & Common Mistakes](#pitfalls--common-mistakes)

---

## How It Works

The **ScriptedScenes** C++ plugin scans a directory for `.lua` files at startup.
Each file becomes its own scene that appears in the REST API and the React web UI
alongside all other C++ scenes.  Existing C++ scenes are completely unchanged.

The Lua environment is intentionally minimal: only the standard `math`, `string`,
and `table` libraries are available.  There is no file I/O, no `os`, no `io`, and
no `require`.  All interaction with the LED matrix goes through the functions
documented below.

---

## Scene File Location

Drop your `.lua` files into:

```
<working directory>/lua_scenes/
```

The working directory is wherever the `main` (or emulator) binary is launched from.
In a packaged install that is typically the same directory as the binary itself (e.g.
`~/led-matrix-v1.x.x/lua_scenes/`).

If the directory does not exist it is created automatically on first run.

The directory is **not** inside the `images/` folder — it lives at the same level:

```
led-matrix-v1.x.x/
├── main              ← matrix binary
├── images/           ← uploaded/processed images
├── lua_scenes/       ← your .lua files go here  ✅
│   ├── plasma.lua
│   └── starfield.lua
└── plugins/
```

> **Tip for development with the emulator:** The emulator is run from
> `<repo root>/emulator_build/install/`, so create
> `emulator_build/install/lua_scenes/` to test your scripts locally.
> `lua` and `sol2` are installed automatically via vcpkg — no
> system-wide Lua installation is required.

---

## Script Contract

### Required globals

| Global | Type | Description |
|--------|------|-------------|
| `name` | string | **Required.** Unique scene identifier used in the REST API and UI. Must not contain spaces (use underscores). |

```lua
name = "my_plasma"
```

### Lifecycle functions

All three functions are optional but strongly recommended.

| Function | When called | Purpose |
|----------|-------------|---------|
| `setup()` | Once, before the first render | Declare configurable properties via `define_property()`. |
| `initialize()` | Once, when the scene is first displayed | Seed random state, pre-compute tables, reset animation state. `width` and `height` are valid here. |
| `render()` | Every frame | Draw pixels. **Must return `true`** to keep running, or `false` to signal the scene is done. |

```lua
name = "example"

function setup()
    define_property("speed", "float", 1.0, 0.1, 5.0)
end

function initialize()
    math.randomseed(42)
end

function render()
    -- drawing code here
    return true
end
```

---

## API Reference

### Read-only globals

These are updated by the C++ host before every call to `render()`.

| Global | Type | Description |
|--------|------|-------------|
| `width` | integer | Matrix width in pixels (typically 128). |
| `height` | integer | Matrix height in pixels (typically 128). |
| `time` | float | Seconds elapsed since the scene started (or since the last hot-reload). |
| `dt` | float | Seconds since the previous frame.  Useful for frame-rate–independent animation. |

### Drawing functions

#### `set_pixel(x, y, r, g, b)`

Set one pixel.  Out-of-bounds coordinates are silently ignored.

| Parameter | Type | Range |
|-----------|------|-------|
| `x` | integer | 0 … width − 1 (left → right) |
| `y` | integer | 0 … height − 1 (top → bottom) |
| `r` | integer | 0 … 255 |
| `g` | integer | 0 … 255 |
| `b` | integer | 0 … 255 |

Values outside 0–255 are clamped automatically.

#### `clear()`

Fill the entire canvas with black (0, 0, 0).  Equivalent to calling
`set_pixel` on every pixel with `r=g=b=0`, but faster.

### Property functions

#### `define_property(name, type, default [, min, max])`

Declare a user-configurable property.  **Must only be called from `setup()`.**
Calling it from `render()` or `initialize()` has no effect.

| Parameter | Type | Notes |
|-----------|------|-------|
| `name` | string | Unique key shown in the web UI. |
| `type` | string | One of `"float"`, `"int"`, `"bool"`, `"string"`, `"color"`. |
| `default` | any | Must match the declared type. |
| `min` | number | Optional.  Only meaningful for `"float"` and `"int"`. |
| `max` | number | Optional.  Only meaningful for `"float"` and `"int"`. |

#### `get_property(name)` → value

Retrieve the current value of a property.  Returns `nil` if the property name
was never registered.

Return type depends on the property type:

| Property type | Lua return type |
|---------------|-----------------|
| `"float"` | number (double) |
| `"int"` | number (double — use `math.floor` if you need an integer index) |
| `"bool"` | boolean |
| `"string"` | string |
| `"color"` | number — 24-bit packed RGB: `0xRRGGBB` |

To unpack a color property:

```lua
local raw = get_property("tint")            -- e.g. 0xFF8000
local r   = math.floor(raw / 65536) % 256
local g   = math.floor(raw / 256)   % 256
local b   = raw % 256
```

#### `log(message)`

Write a line to the spdlog output (INFO level).  Useful for debugging.

```lua
log("Current speed: " .. tostring(get_property("speed")))
```

---

## Property Types

| Type | Lua default example | Notes |
|------|---------------------|-------|
| `"float"` | `1.0` | Supports `min`/`max` constraints. |
| `"int"` | `8` | Supports `min`/`max` constraints. |
| `"bool"` | `true` | Shown as a checkbox in the web UI. |
| `"string"` | `"hello"` | Free-form text input. |
| `"color"` | `0xFF0000` | 24-bit packed RGB integer. Shown as a colour picker. |

---

## Hot-Reload

While the matrix (or emulator) is running, you can edit a `.lua` file and save it.
The C++ host checks the file's modification time at the start of every frame.
When a change is detected:

1. The Lua state is torn down and rebuilt.
2. The script is re-executed.
3. `setup()` is called again (property *shapes* are fixed — adding new properties
   in a reload has no effect; existing property values configured via the web UI
   are preserved).
4. `initialize()` is called again.
5. The frame timer resets to `time = 0`.

This means you can tweak your rendering algorithm and see the result in under a
second without restarting the server.

> **Note:** Hot-reload is always active.  If you want a clean state (e.g. after
> changing a property `type`), restart the matrix process.

---

## Error Handling

| Situation | Behaviour |
|-----------|-----------|
| `.lua` file has a syntax error when the server starts | Scene is **skipped** (not registered). An error is logged. |
| `setup()` throws a runtime error | Error is logged; other scenes continue normally. |
| `render()` throws a runtime error | Error is logged once per occurrence; the scene keeps running (canvas is left unchanged for that frame). |
| File disappears while running | Filesystem error is caught silently; the scene skips that frame. |

All errors are written to the spdlog output.  When running the emulator you can
see them on stdout.  On the Pi, check `journalctl` or the log file if you have
configured one.

---

## Examples

Four ready-to-use scripts are included in the source tree at:

```
plugins/ScriptedScenes/matrix/examples/
├── plasma.lua       – classic plasma wave with configurable speed & scale
├── starfield.lua    – 3-D star warp with configurable speed
├── colour_bars.lua  – scrolling colour bars with configurable count & speed
└── ripple.lua       – concentric ripples from a bouncing centre point
```

Copy any of these into your `lua_scenes/` directory to get started:

```bash
cp plugins/ScriptedScenes/matrix/examples/plasma.lua \
   emulator_build/install/lua_scenes/
```

### Minimal working scene

```lua
name = "solid_red"

function render()
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            set_pixel(x, y, 255, 0, 0)
        end
    end
    return true
end
```

### Scene with properties

```lua
name = "pulsing_color"

function setup()
    define_property("speed", "float", 2.0, 0.1, 10.0)
    define_property("tint",  "color", 0x00FFFF)
end

function render()
    local raw  = get_property("tint")
    local tr   = math.floor(raw / 65536) % 256
    local tg   = math.floor(raw / 256)   % 256
    local tb   = raw % 256

    local bright = (math.sin(time * get_property("speed")) + 1.0) / 2.0

    local r = math.floor(tr * bright)
    local g = math.floor(tg * bright)
    local b = math.floor(tb * bright)

    for y = 0, height - 1 do
        for x = 0, width - 1 do
            set_pixel(x, y, r, g, b)
        end
    end
    return true
end
```

---

## Pitfalls & Common Mistakes

### 1. Forgetting `return true` in `render()`
If `render()` returns nothing (or `false`), the scene is considered finished and
will be rotated away immediately.  Always end with `return true` unless you
intentionally want the scene to stop.

### 2. Pixel coordinates are 0-based
`x` runs from `0` to `width - 1`; `y` from `0` to `height - 1`.  Using
`for x = 1, width do` will miss column 0 and write one pixel off the right edge
(silently ignored).

### 3. Colours are integers, not floats
`set_pixel` expects integer values 0–255.  Passing a float like `127.5` will
work (it is clamped), but using `math.floor()` is clearer and avoids surprises.

### 4. `define_property` only works inside `setup()`
Calling `define_property` from `render()` or `initialize()` is silently ignored.
All property declarations must happen in `setup()`.

### 5. `get_property` returns a Lua `number`, not an integer
Even `int` properties come back as Lua `number` (double).  If you use the value
as a table index or loop bound, wrap it with `math.floor()`:

```lua
local count = math.floor(get_property("bar_count"))
for i = 1, count do ... end
```

### 6. `require`, `io`, `os`, `dofile` are not available
The Lua sandbox only exposes `base`, `math`, `string`, and `table` libraries.
There is no file I/O or module loading.  If you need shared logic, copy it into
each script or use Lua's module pattern with local functions.

### 7. Expensive per-pixel inner loops
The matrix is 128 × 128 = 16 384 pixels.  A plain Lua loop over every pixel
runs in interpreted bytecode and is noticeably slower than equivalent C++ code.
Keep inner loops short or reduce resolution by stepping by 2 or 4 pixels.  If
you need maximum performance, write a C++ plugin instead (see
[PLUGIN_DEVELOPMENT.md](PLUGIN_DEVELOPMENT.md)).

### 8. Hot-reload does not change property *shapes*
If you add, remove, or rename a `define_property` call and save the file, the
change takes effect for *new* scene instances (e.g. after the server restarts)
but not for the currently-running one.  The live instance keeps its original
property list.  Restart the server to pick up structural changes.

### 9. `name` must be globally unique across all scenes
If two `.lua` files declare the same `name`, only one will appear in the scene
list (the last one loaded wins).  Use descriptive, unique names and consider
prefixing them with `lua_` to avoid collisions with C++ scene names.

### 10. Script not appearing after copying
The directory is scanned only at startup.  Dropping a new `.lua` file into
`lua_scenes/` while the server is running has no effect until the next restart.
Hot-reload only refreshes *existing* scenes, not new files.
