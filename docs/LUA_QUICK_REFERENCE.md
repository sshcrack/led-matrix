# Lua Scene Development - Quick Reference

## 1. File Setup & Execution
* **Location:** `<working directory>/lua_scenes/`
* **Loading:** Directory is scanned only on startup (new files require a server restart).
* **Hot-Reload:** Saving an *existing* running script automatically rebuilds the state and updates logic (note: property structures won't update without a restart).

## 2. Script Contract
* **`name`** (Global String): **Required**. Must be unique, no spaces (e.g., `name = "my_scene"`).
* **`external_render_only` (Global bool): **Optional**. If true, marks this scene as heavy computation and will use an external render source instead of the RPI
* **`setup()`**: Called once. **Must** be used for declaring properties (`define_property`).
* **`initialize()`**: Called when the scene is first displayed. Use for RNG seeding or state resets. `width` and `height` are accessible here.
* **`render()`**: Called every frame. **Must end with `return true`**, or the scene will terminate.

## 3. API Reference
**Globals (Read-only, updated per frame):**
* `width`, `height`: Canvas dimensions (0-based, usually 128x128).
* `time`: Seconds elapsed since scene start/reload.
* `dt`: Delta time (seconds since last frame).

**Drawing:**
* `set_pixel(x, y, r, g, b)`: x (0 to width-1), y (0 to height-1). Colors 0-255. Out-of-bounds coordinates are safely ignored.
* `clear()`: Rapidly fills the canvas with black (0,0,0).

**Properties & Logging:**
* `define_property(name, type, default [, min, max])`: Types include `"float"`, `"int"`, `"bool"`, `"string"`, `"color"`. Valid *only* inside `setup()`.
* `get_property(name)`: Retrieves configured values. 
* `log(message)`: Outputs to standard spdlog (INFO).

## 4. Critical Pitfalls & Restrictions
* **No standard I/O:** `require`, `io`, `os`, and `dofile` are disabled. Only `math`, `string`, and `table` are available.
* **Type coercion:** `get_property("int")` returns a Lua double. Wrap in `math.floor()` for loops or array indexing.
* **Math.pow deprecation:** Use `^` operator instead of `math.pow` for exponentiation (e.g., `2 ^ 3` instead of `math.pow(2, 3)`).
* **Color extraction:** `"color"` properties return a 24-bit integer (`0xRRGGBB`). Unpack them manually:
  ```lua
  local raw = get_property("tint")
  local r = math.floor(raw / 65536) % 256
  local g = math.floor(raw / 256) % 256
  local b = raw % 256
```