# Tier 2 - Medium Priority

## C1. `using namespace std;` in headers

`shared/matrix/include/shared/matrix/config/MainConfig.h` and `shared/matrix/include/shared/matrix/config/data.h`. Remove and qualify with `std::` (fix includers that relied on it).

## C15. Inconsistent API URL conventions

`src_matrix/server/*.cpp`: `/api/` prefix only on custom-assets and update routes; everything else (`/set_active`, `/list_scenes`, `/flash`, ...) is unprefixed, and some GET endpoints mutate state (`/set_active`, `/toggle`, `/skip`). Pick one convention and use POST/PUT/DELETE for mutations (needs a web UI change; the request guard assumes browser requests are same-origin).
