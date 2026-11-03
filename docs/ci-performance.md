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

## Tool drift and cache scope

The first master run after PR #85 used a newer hosted runner image. Its `fmt`
package ABI metadata differed solely in CMake version (4.4.4 to 4.4.5) and the
resulting host-tool dependency hashes. `VCPKG_FORCE_DOWNLOADED_BINARIES=1` makes
vcpkg use the tool versions from its fixed metadata instead of newer host tools.
That causes one initial rebuild, then keeps package keys stable across host
CMake updates. Compiler changes still correctly invalidate affected packages.

GitHub cannot restore PR-scoped caches on master. Master seeds its own compiler
cache, which subsequent PRs can read. The closed-PR cleanup workflow removes only
that PR's merge-ref caches; it never checks out PR code or deletes master caches.
NuGet remains a shared package cache independent of GitHub branch scope.

Cache diagnostics publish sccache statistics and counts of backend error categories.
Raw logs, which may contain signed URLs, are not uploaded. A successful upload is
not treated as proof of a cache hit; warm-run restore/hit statistics must verify it.

Build jobs use a shallow checkout, including the required PortAudio submodule.
Frontend builds fetch one commit and no C++ submodules. Artifact comments use the
Actions metadata API directly, so they neither clone the repository nor download
all of the build archives simply to list them. Full history remains available in
the release workflow where changelog generation uses tags and commit dates.

NuGet-restored packages are not written into vcpkg's files-cache provider. CI also
caches the installed dependency tree, so a warm job can restore one GitHub archive
instead of downloading every NuGet package again. That tree uses an exact key
covering preset, runner image and dependency/overlay/toolchain inputs; it has no
broad restore prefix that could reuse an incompatible installed tree. The binary
archive provider retains its broader fallback because vcpkg validates those ABI
keys before selecting individual packages. Bump the installed-cache generation
if dependency-affecting configure flags change outside the hashed preset inputs.

Plugin-owned overlays participate in both cache keys. An exact installed-tree
hit skips the NuGet network probe; otherwise a failed probe is diagnostic and
vcpkg can still satisfy dependencies from GitHub archives or source builds.

PR compiler caches are read-only and reuse master objects; master builds publish
new compiler objects. This avoids duplicating full compiler caches into each
isolated PR scope while still retaining PR dependency snapshots. Modified sources
can compile normally in a PR; after merging, their objects join the shared master
cache. No paid cache-storage limit increase is required by this configuration.

## Source transfer

Profiling the warm PR #88 run showed three network costs outside compilation:

- `actions/checkout` spent 90-110 s on most jobs. About 50 MiB of the 94 MiB
  tree is README/docs GIFs (`plugins/*/img/`, `docs/*.gif`) that no build reads.
  Build jobs use a non-cone sparse checkout that excludes them; sparse checkout
  also makes `actions/checkout` use a blobless partial clone, so excluded blobs
  are never downloaded. `scene_previews/` stays because packages install it.
- Submodules pinned to non-tip commits are fetched with an explicit
  `git submodule update --init --depth 1` after checkout.
- `lukka/run-vcpkg` cloned the full microsoft/vcpkg history, and vcpkg's git
  default registry fetched the full history again into its registry cache
  (`Fetching registry information ... (HEAD)`: 80-115 s). CI now seeds both
  with depth-1 fetches of the pinned baseline (plus registry `HEAD`) and points
  `X_VCPKG_REGISTRIES_CACHE` at the seeded cache. Every port version resolved
  from the baseline is a tree inside the baseline commit, so the install plan
  and package ABIs are unchanged; a local dry run produced an identical plan to
  the full-history registry. A future `overrides` entry or `version>=`
  constraint newer than the baseline would need a deeper registry fetch.

When the exact installed-dependency cache hits, CI skips Mono setup, the NuGet
key/probe steps, and removes NuGet from `VCPKG_BINARY_SOURCES`: nothing needs to
be restored or published. A miss uses the full NuGet + GitHub archive path.
