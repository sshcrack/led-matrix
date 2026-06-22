# Tech Debt & Refactoring Plan

Four tiers, ordered by severity. Each file is self-contained so agents can execute them independently.

| Tier | File | Focus |
|------|------|-------|
| **0** | `tier0-critical.md` | UB, data races, correctness bugs |
| **1** | `tier1-high.md` | Architectural debt, maintainability blockers |
| **2** | `tier2-medium.md` | Code quality, performance, build system |
| **3** | `tier3-low.md` | Style, naming, minor cleanup |

## Conventions

- **Do not add comments** unless the fix requires them for correctness.
- **Follow existing code style** (snake_case for functions/variables, PascalCase for classes).
- **No emojis** in code or comments.
- **Run** `cmake --preset emulator && cmake --build --preset emulator` after non-trivial C++ changes to verify compilation.
- **Run** `cmake --build --preset emulator --target install` before testing.
- **Commit** changes only when explicitly asked.
