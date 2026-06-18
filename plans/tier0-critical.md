# Tier 0 — Critical (fix immediately)

UB, data races, and real correctness bugs that can cause crashes or corruption at runtime.

---

## 1. Signal handler calls non-async-signal-safe functions

**Files:** `src_desktop/main.cpp:104-122`

**Problem:** The Linux `SIGINT`/`SIGTERM` handler calls `spdlog::info()`, `fmt::format()`, and `cpr::Get()` — none of which are async-signal-safe. This is undefined behavior.

**Also:** `static bool shouldExit` on line 64 is not `volatile sig_atomic_t`, which is required for signal handler communication.

**Fix:**
1. Change `static bool shouldExit` → `static std::atomic<bool> shouldExit{false}` (or `volatile sig_atomic_t`).
2. Move all I/O (spdlog, cpr::Get) out of the signal handler. Instead, just set the flag.
3. Create a dedicated monitoring thread (or poll in the main loop) that checks the flag and performs the shutdown HTTP call (`cpr::Get` to turn off the matrix) on the main thread.
4. Same approach on Windows for `console_ctrl_handler` (lines 76-95) — calling spdlog/cpr from a console control handler is unsafe.

**Verify:** No spdlog, cpr, or fmt calls in any signal/control handler. All I/O happens on normal threads.

---

## 2. DBus `dbus_message_append_args` passes `std::string*` as `const char**`

**File:** `src_desktop/single_instance_manager.cpp:111`

**Problem:**
```cpp
dbus_message_append_args(msg, DBUS_TYPE_STRING, &(_appId), DBUS_TYPE_INVALID);
```
`_appId` is a `std::string`. `&(_appId)` is a `std::string*`, but `DBUS_TYPE_STRING` expects a `const char**`. This passes the raw `std::string` object layout where a `const char*` pointer is expected — UB.

**Fix:**
```cpp
const char* cstr = _appId.c_str();
dbus_message_append_args(msg, DBUS_TYPE_STRING, &cstr, DBUS_TYPE_INVALID);
```

---

## 3. C-style cast reinterprets `shared_ptr` internals

**File:** `shared/matrix/src/shared/matrix/config/data.cpp:46-47`

**Problem:**
```cpp
const auto &item = n["scene"];
const json &j_scene = item;
const auto *list = (const Scenes::Scene *&)j_scene.get_ptr<const json *>()->get<std::shared_ptr<Scenes::Scene>>();
```
The C-style cast `(const Scenes::Scene *&)` reinterprets the `shared_ptr`'s internal representation as a raw pointer. This is UB — the `shared_ptr` implementation layout is not guaranteed.

**Fix:** Call `.get()` on the shared_ptr explicitly instead of the C-style cast.

---

## 4. `map::operator[]` inserts null entry on missing key

**Files:** `shared/matrix/src/shared/matrix/config/MainConfig.cpp:61` (and likely line 239)

**Problem:**
```cpp
this->data.presets[data.curr]
```
If `data.curr` is not in the `presets` map, `operator[]` default-constructs a nullptr entry, permanently corrupting the config.

**Fix:** Replace with `this->data.presets.at(data.curr)` or use `find()` with explicit error handling and logging.

Check line 239 (`get_curr()`) for the same pattern.

---

## 5. `get_plugin_configs() const` can't lock non-mutable mutex

**File:** `shared/matrix/src/shared/matrix/config/MainConfig.cpp:233-235`

**Problem:** This `const` method returns a copy of `pluginConfigs` without synchronization because the mutex is not `mutable`. A concurrent `set_plugin_config()` from another thread causes a data race.

**Fix:** Make `data_mutex` `mutable`. The method is logically const (it only reads), so this is correct.

---

## 6. `server_running` is plain `bool`, not atomic

**File:** `src_matrix/udp.cpp:21`

**Problem:** `server_running` is a plain `bool` read from the worker thread and written from the main thread (constructor/destructor). This is a data race.

**Fix:** Change to `std::atomic<bool> server_running{false}`.

---

## 7. UDP server thread starts before constructor finishes

**File:** `src_matrix/udp.cpp:164-165`

**Problem:**
```cpp
server_running = true; // line 164
std::thread(&UdpServer::server_loop, this) // line 165
```
The thread starts potentially before `server_running` is set. Also, the thread accesses `this->udp_socket` while the constructor is still setting it up.

**Fix:** Move the `std::thread` creation to after all setup is complete, and set `server_running = true` before spawning the thread (or let the thread constructor be the last thing).

---

## 8. `hardware.cpp` deletes matrix allocated in `main.cpp`

**Files:** `src_matrix/main.cpp:153`, `src_matrix/matrix_control/hardware.cpp:69`

**Problem:** `main.cpp` creates the matrix via `MatrixFactory::CreateMatrix(options)` and passes the raw pointer to `hardware_mainloop()`, which deletes it. Cross-file ownership with no RAII — if an exception prevents `start_hardware_mainloop()` from being called, the matrix leaks. If it's called multiple times, double-free.

**Fix:** Wrap the matrix in a `std::unique_ptr<rgb_matrix::RGBMatrixBase>` in `main.cpp`, and pass a raw/non-owning pointer to `hardware_mainloop()`. Remove the `delete matrix;` from `hardware.cpp`.

---

## 9. Update routes: fire-and-forget thread with use-after-free risk

**File:** `src_matrix/server/update_routes.cpp:91-101`

**Problem:**
```cpp
std::thread(...).detach();
```
Inside the detached thread, `update_manager` (a raw pointer) is accessed. If the server shuts down while the thread runs, the pointer dangles.

**Fix:** Make the thread joinable and track it (e.g., a member `std::thread` that's joined on shutdown). Or use a thread pool. At minimum, ensure the thread is joined before `update_manager` is destroyed.

---

## 10. `WebsocketClient` destructor calls `initNetSystem` instead of `uninitNetSystem`

**File:** `shared/desktop/src/shared/desktop/WebsocketClient.cpp:48-56`

**Problem:** The destructor calls `ix::initNetSystem()` — likely a copy-paste bug from the constructor. The corresponding deinit is never called.

**Fix:** Call the proper ixwebsocket shutdown function (likely `ix::uninitNetSystem()` or whatever the library provides for cleanup). If no such function exists, remove the call entirely.

---

## 11. Desktop config `getHostname()` returns dangling reference

**File:** `shared/desktop/src/shared/desktop/config.cpp:108-111`

**Problem:**
```cpp
const std::string& General::getHostname() const {
    std::shared_lock lock(mutex_);
    return hostname;
} // mutex released here, caller holds dangling reference
```

**Fix:** Return by value (`std::string`) like `getHostnameCopy()` does. Review all callers of `getHostname()` to ensure they're not relying on the reference.
