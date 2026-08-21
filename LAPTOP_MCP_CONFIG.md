# Laptop MCP configuration

This repository's `.laptop-mcp.toml` is the source of truth for fresh Laptop MCP sandboxes.

## Why this project needs custom configuration

- The CMake/vcpkg build needs Linux GUI, Wayland/X11, DBus and audio development headers that are not all part of the managed base image.
- `plugins/AudioVisualizer/thirdparty/portaudio` is a required Git submodule, so sandboxes use recursive submodules.
- PulseAudio development/runtime utilities are present so the desktop audio path can build and `parec`-based loopback diagnostics can run in the sandbox when an audio server is reachable.
- The default 8 GiB memory limit is intentional for C++/vcpkg and web builds without reserving host CPUs. Do not add a default CPU pin: multiple Laptop MCP agents may work concurrently.

## Fresh-sandbox workflow

1. Read `LAPTOP_MCP_SANDBOX.md` first; Laptop MCP generates it for every new sandbox.
2. Follow `AGENTS.md` for repository workflow and validation.
3. Use the checked-in CMake presets and `/opt/vcpkg` (`VCPKG_ROOT` is provided by Laptop MCP).
4. Build `react-web` with its checked-in pnpm lockfile when web changes are involved.

Do not add build directories (`build`, `emulator_build`, `desktop_build`), `vcpkg_installed`, `react-web/node_modules`, secrets, or local audio configuration to `workspace.copy`. Those are disposable outputs or machine-specific state.

After changing `.laptop-mcp.toml`, run `laptop-mcp doctor` from the source checkout and restart/refresh the repository worker so new sandboxes use the new derived image.
