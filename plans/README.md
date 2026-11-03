# Tech Debt Plan

Remaining items after the June audit (tier 0 is fully resolved).

| Tier | File | Focus |
|------|------|-------|
| **1** | `tier1-high.md` | Global state, shutdown race |
| **2** | `tier2-medium.md` | Headers, API conventions |
| **3** | `tier3-low.md` | Naming, docs, formatting CI |

## Conventions

- Do not add comments unless needed for correctness; follow the existing style (snake_case functions, PascalCase classes); no emojis.
- Build with the emulator preset after non-trivial C++ changes. Commit only when asked.
