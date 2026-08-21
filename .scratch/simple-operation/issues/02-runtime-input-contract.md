# Runtime inputs are automatic capabilities

Type: task
Status: resolved
Blocked by: 01

## Question

What runtime-input contract lets plugins publish machine state once, lets scenes declare requirements without bespoke connection checks, and gives later directors/modulation a stable source of availability and values?

## Resolution

Implemented on top of `c974928 smooth audio waveform and separate mirror layers`. Runtime Inputs now provide typed availability/signals, TTL-based freshness, scene required/optional declarations, scheduler eligibility, live required-input loss handling, diagnostics/API exposure, preview-manifest exposure, and web API types. Initial producers are desktop connectivity, rich audio analysis, and Spotify playback.

Validated with the emulator build plus `runtime_inputs_smoke`, `property_compat_smoke`, `scene_registry_smoke`, `audio_scene_preview_smoke`, and `declared_preview_provider_smoke`; all pass. The React production build also passes.

## Polish review

Completed after review. Generic audio Runtime Input publication is throttled to 5 Hz so the Pi does not rebuild string-keyed signal maps at the 60 Hz desktop packet rate; full-rate visual consumers continue using typed `AudioState`. Plugins now declare production Runtime Input ids and registry validation rejects orphaned required inputs. Direct `runtime_selection_smoke` coverage verifies scheduler eligibility and retirement after live input loss. Full emulator CTest, React production build, and ARM64 Pi cross-build pass.
