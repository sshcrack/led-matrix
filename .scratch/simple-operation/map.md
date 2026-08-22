# Wayfinder Map — Simple Automatic Operation

## Destination

The matrix can be configured once and then operate beautifully on its own: scene choice adapts to available runtime data, desktop/music state, and performance without the user managing scene weights, preset swaps, transition mechanics, or compositor layers. Manual presets remain an advanced escape hatch.

## Notes

- Execution is intentionally carried into this map because the user asked to plan the multi-session effort and immediately start Phase 1.
- Default UX is automatic; configuration complexity belongs behind the director.
- The Pi remains the authoritative renderer. Web/Scene Lab preview demand yields first under render pressure.
- A compositor may exist internally later, but must not create required manual setup.
- Global palette/color architecture is a later pass, not part of this map.
- Deterministic visual-regression CI is explicitly deferred.
- Consult `wayfinder`, `domain-modeling`, `codebase-design`, and `frontend-design` where relevant.

## Current execution state

- Active sandbox: `auto-director-rebased`.
- Base includes `c974928 smooth audio waveform and separate mirror layers`.
- Phase 1 — Runtime Inputs + eligibility foundation: **complete and review-polished**.
- Phase 2 — Scene descriptors, curated Scene Variants, and manifest/UI exposure: **complete and review-polished**.
- Phase 3 — Automatic/Ambient Director policy + default Automatic Mode UX: **complete and review-polished**.
- Phase 4 — Music Director + transition intelligence: **complete and review-polished**.
- Next phase: **Scene Lab runtime and preview policy** (`issues/06-scene-lab-runtime-policy.md`).
- After that: observability/repeatability.
- Do not turn the compositor into user-facing configuration. If composition is added later, directors should own it automatically.
- Do not start the global palette/color-system pass yet.

### “Continue with the next phase” instruction

When asked to continue with the next phase, resume this map, inspect the completed Runtime Input foundation rather than redesigning it, and start `04-scene-descriptors-and-variants.md`. Use the existing scene/property/preview/plugin metadata as migration inputs. Prefer a small extensible descriptor contract plus a few curated variants across representative scene families, then wire it into the manifest/API and web UI before expanding coverage.

## Decisions so far

- [Default operation is automatic](issues/01-default-operation-is-automatic.md) — users choose intent; the runtime chooses eligible scenes, weights, transitions, and data-dependent behavior.

## Not yet specified

- How much manual override should remain visible in the default UI once Automatic Mode exists.
- Whether the internal compositor is needed for the first automatic-director release or can wait until directors have concrete composition use cases.
- Exact global palette semantics and contrast rules; intentionally deferred to a later effort.

## Out of scope

- Visual-regression/golden-frame CI for this roadmap.
- A user-facing node/compositor graph or mandatory manual layer configuration.
