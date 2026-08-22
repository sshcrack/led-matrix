# Scene Lab runtime and preview policy

Type: prototype
Status: resolved
Blocked by: 02, 04

## Question

How should Scene Lab render and edit scenes on the Pi while guaranteeing display stability, including preview FPS degradation, optional playback pausing, temporary properties, and save-as-variant/preset behavior?

## Resolution

Scene Lab now uses the Pi/emulator matrix renderer as the single authoritative render path. Starting a Lab session temporarily replaces normal Automatic/Manual playback with a fresh temporary scene, caps the Lab scene to 10–30 FPS, and reuses the existing pull-only live-frame WebSocket; merely opening the WebSocket still performs no matrix capture. Sessions carry a 45-second lease refreshed by a browser heartbeat and automatically relinquish playback when the lease expires. Runtime Input requirements remain enforced while the Lab is active.

Temporary controls are canonicalized against registered scene properties so unknown keys are rejected instead of being silently echoed. Lab state can be saved either as a persistent custom Scene Variant or as a manual preset. Saved variants are merged into `/list_scenes` and remain loadable from presets even though they are not compiled into the plugin. Saving a new look immediately adopts its variant ID in the active Lab session, so a subsequent preset save preserves the relationship.

Review fixes included fresh-install default preset construction from declared property defaults, canonical property persistence, invalid-property crash prevention, non-resurrecting expired heartbeats, and custom-variant round-trip preservation. End-to-end validation exercised REST start/update/heartbeat/save/stop, Runtime Input reporting, 45-second lease expiry, clean daemon shutdown, and a non-black live frame pulled from the actual matrix renderer.
