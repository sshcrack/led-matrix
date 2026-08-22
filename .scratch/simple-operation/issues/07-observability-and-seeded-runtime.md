# Runtime observability and repeatability

Type: task
Status: resolved
Blocked by: 02

## Question

Which performance telemetry, scene-selection reasoning, seeded-randomness hooks, and runtime diagnostics must be exposed so automatic operation is explainable and reproducible without burdening normal users?

## Resolution

Automatic Director randomness now uses a persistent non-zero 64-bit seed stored in the main config. Reapplying the same seed intentionally resets the in-process Director sequence; generating a new seed persists it immediately. Director diagnostics expose the exact seed as a decimal string, decision count, current context targets, render-quality budget, recent history, ranked candidates, scores, and selection reasons. Automatic transition diagnostics expose from/to scene variants, chosen effect, duration, beat-alignment delay/state, and reason. Diagnostics also expose effective automatic/manual state and Scene Lab ownership without adding configuration to the normal control surface.

The advanced Diagnostics/Health UI shows this reasoning and provides explicit reproduce/new-seed controls. Repeatability is covered by a multi-decision seeded sequence test plus config reload and same-seed reset tests. Live integration verified transition telemetry and exact seed persistence across a clean daemon restart.
