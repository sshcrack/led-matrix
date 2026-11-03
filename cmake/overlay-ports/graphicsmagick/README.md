This overlay mirrors the vcpkg `graphicsmagick` 1.3.49 port proposed in
<https://github.com/microsoft/vcpkg/pull/54862>: same manifest, build logic and
patches. Once that PR is merged and the builtin baseline includes it, this
overlay can be dropped.

The only difference is source acquisition. The overlay downloads the official
signed release tarball from SourceForge instead of Heptapod's generated archive,
because Heptapod's anti-bot HTML response made Windows CI fail hash validation.

Release provenance was verified using the detached `.tar.xz.asc` signature from
the same official release directory and the key published on GraphicsMagick's
security page. GnuPG reported a good signature with fingerprint
`EBDFDB21B020EE8FD151A88DE301047DE1198975`, matching the fingerprint documented at
<https://graphicsmagick.sourceforge.io/download.html>. The patched `configure.ac`
and `magick/studio.h` are identical to those from the Heptapod
`GraphicsMagick-1_3_49` archive used by the vcpkg port.

Release: <https://sourceforge.net/projects/graphicsmagick/files/graphicsmagick/1.3.49/>

SHA-512:
`ad721c9b57fe94a1d46a6d051156c5072d24742d974650c9200877b813d3a36d2154a8c25c40af3d06fb8f91c0fa4688905b52f49f49ea2c87d627bedb101d3a`

## Why 1.3.49 (Windows x64)

1.3.45 had three problems on `x64-windows`. Each one stopped the scene worker
from using images:

1. **Crash in `InitializeMagick()`.** 1.3.45's `magick/magick_types.h.in`
   checked `WIN64` but not `_WIN64`, so `magick_uintptr_t` was 32-bit and
   `MagickMallocAligned()` truncated heap pointers above 4 GiB. 1.3.46+ uses
   `<stdint.h>`, so the old `magick-types.diff` is gone.
2. **No coders registered.** `magick/studio.h` enables loadable-module mode
   whenever `_DLL` is defined, and MSVC defines that for every `/MD` build.
   `msvc-static-coders.diff` also requires `_VISUALC_` (only GraphicsMagick's
   VisualMagick projects define it), so the built-in coders get registered.
   Upstream: <https://sourceforge.net/p/graphicsmagick/bugs/782/>.
3. **Missing libraries.** On 1.3.45, configure dropped JPEG on Windows. On
   1.3.49 the PNG check picked a nonexistent `-lpng16`, which broke every later
   library check. `dependencies.diff` accepts the MSVC `-llibpng16(d)` name.
   Upstream: <https://sourceforge.net/p/graphicsmagick/bugs/781/>.

Verified on `x64-windows` (MSVC 14.44) and `x64-linux`: a Magick++ program runs
`InitializeMagick()` and round-trips PNG, WebP, JPEG, TIFF and GIF.
