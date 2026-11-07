# LED Matrix

A passive ambient display whose software should make visually strong choices automatically while keeping manual controls available for advanced tuning.

## Language

**Runtime Input**:
Named machine state that is published automatically by the runtime or a plugin and can be consumed by scenes or directors, including both availability and optional signals.
_Avoid_: Feed, manual input, preview input

**Automatic Mode**:
The normal operating mode where the system chooses eligible scenes and presentation behavior from runtime context instead of requiring the user to curate weights and switch presets manually.
_Avoid_: Smart preset, auto preset

**Director**:
A policy that chooses and sequences visual behavior from scene descriptors and Runtime Inputs while hiding low-level selection mechanics from normal operation.
_Avoid_: Scheduler when referring to product-level choice policy

**Scene Variant**:
A curated named configuration of a scene intended to look good without manual property tuning.
_Avoid_: Duplicate scene, preset when referring to one scene configuration

**Scene Lab**:
The web experience for trying a scene or Scene Variant against the Pi renderer with temporary controls and live preview before saving it.
_Avoid_: Editor, compositor

**Preset Persistence**:
The browser-side owner of every preset read and write (`react-web/src/core/presetPersistence.ts`): writes for one preset run in order, a read waits for that preset's queued writes, deleting holds writes back, and acknowledged writes update the shown preset list. Scope is one browser session.
_Avoid_: Save queue (that is its internal mechanism), preset service

**Desktop Handoff**:
The matrix-side owner of the desktop render session across scene and transition phases (`src_matrix/matrix_control/DesktopHandoff.h`): adopting a session a transition warmed, deciding remote/hold/local per frame, and abandoning or falling back when the desktop scene worker stalls or disappears. Render loops ask it where frames come from; they do not read worker state themselves.
_Avoid_: Offload manager, remote renderer (RemoteRender is its transport)

**Plugin Activation**:
Loading the plugin web UIs once per page (`createPluginActivation` in `react-web/src/core/pluginRegistry.ts`): a plugin's registration and its stylesheet are applied together, only after its module loaded and passed the UI API check.
_Avoid_: Plugin registration (only the last step)

## Current roadmap state

The six-phase simple-operation roadmap is complete in sandbox `auto-director-rebased`: Runtime Inputs, scene descriptors/curated Scene Variants, default Automatic Mode, music-aware transition intelligence, Pi-authoritative Scene Lab, and seeded runtime observability/repeatability are implemented and review-polished. Do not redo those foundations when continuing. The global palette/color-system pass remains intentionally deferred.
