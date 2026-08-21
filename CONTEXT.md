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

## Current roadmap state

Phase 1 (Runtime Inputs and automatic eligibility) is complete in sandbox `auto-director-rebased`. The next phase is Scene descriptors + curated Scene Variants + richer plugin/scene manifests. A future agent told to “continue with the next phase” should start there, not redo Runtime Inputs.
