# CI caches and release reuse

Ubuntu 26.04's Mono 6.14 packages omit `ca-certificates-mono` and `cert-sync`,
leaving its TLS store empty. Restoring apt package files also does not run trust-
store hooks. NuGet then spent minutes failing TLS verification and rebuilt dependencies.
A container reproduction against the real feed produced
`CERTIFICATE_VERIFY_FAILED` with an empty Mono store and succeeded immediately
after `cert-sync`. The workflow now installs Mono normally and synchronizes both
machine and user stores; synchronization errors fail the job. When the distro
omits `cert-sync`, the setup script builds Mono's own MIT-licensed tool from a
pinned, SHA256-verified source against the installed runtime. A bounded NuGet
search checks the real feed TLS path before dependency restoration.

Upstream packaging regression: https://github.com/KSP-CKAN/CKAN/issues/4590

The NuGet backend is BaGetter at `storage:5001`, reverse proxied by Nginx on `pi`.
The origin certificate and public Cloudflare certificate validate with the system
CA bundle. Nginx already permits 2 GB requests. Cloudflare's proxy can still limit
large uploads; GitHub-backed vcpkg binary archives provide a second cache for those
packages without requiring public DNS changes. The dependency cache key includes
the hosted runner image generation and toolchain/manifest inputs, so a compiler
upgrade can save a new cache instead of repeatedly restoring an obsolete one. No server restart was needed for
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
errors fall back to the normal complete release build. Release tags explicitly
target the workflow commit, even if the default branch advances meanwhile.

References:
- https://www.mono-project.com/docs/faq/security/
- https://github.com/awalsh128/cache-apt-pkgs-action#non-file-dependencies
- https://github.com/mozilla/sccache/blob/main/docs/GHA.md
- https://learn.microsoft.com/en-us/vcpkg/reference/binarycaching
