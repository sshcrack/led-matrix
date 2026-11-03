# CI caches and release reuse

The old Linux cache restored Mono package files without initializing its trust
store. NuGet then spent minutes failing TLS verification and rebuilt dependencies.
A container reproduction against the real feed produced
`CERTIFICATE_VERIFY_FAILED` with an empty Mono store and succeeded immediately
after `cert-sync`. The workflow now installs Mono normally and synchronizes both
machine and user stores; synchronization errors fail the job.

The NuGet backend is BaGetter at `storage:5001`, reverse proxied by Nginx on `pi`.
The origin certificate and public Cloudflare certificate validate with the system
CA bundle. Nginx already permits 2 GB requests. Cloudflare's proxy can still limit
large uploads; GitHub-backed vcpkg binary archives provide a second cache for those
packages without requiring public DNS changes. No server restart was needed for
the reproduced TLS problem.

Application compilation uses sccache with the GitHub Actions backend. Compiler,
flags, input contents and target architecture participate in compiler cache keys.
Windows uses embedded debug information so MSVC compilations can be cached.
Inspect the sccache job summary for hits and misses; the first run seeds the cache.

The normal emulator job builds and runs the complete suite. The sanitizer job
builds only the selected regression executables and their dependencies, including
all plugins used by the ambiance tests. It retains ASan leak detection and UBSan.

After every successful matrix, a `build-provenance` artifact records the checked
out Git tree and package version. Release preparation searches successful CI runs
from this repository for an exact tree/version match with all three package
artifacts still available. A matching run is reused, including after a squash
merge with a different commit ID. Missing/expired/mismatched artifacts or lookup
errors fall back to the normal complete release build.

References:
- https://www.mono-project.com/docs/faq/security/
- https://github.com/awalsh128/cache-apt-pkgs-action#non-file-dependencies
- https://github.com/mozilla/sccache/blob/main/docs/GHA.md
- https://learn.microsoft.com/en-us/vcpkg/reference/binarycaching
