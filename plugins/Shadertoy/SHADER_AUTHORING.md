# Authoring LED Matrix Shadertoy scenes

This plugin intentionally keeps shaders close to normal Shadertoy GLSL so a person or coding agent can add a scene by creating one `.frag` file. No C++ renderer registration is required.

- **Repo/built-in shader:** add `plugins/Shadertoy/shaders/<name>.frag`. It is packaged automatically and appears as `shader:<name>`. This is the preferred path for AI-authored visuals that belong in the codebase.
- **Runtime/user shader:** put `<name>.frag` in `data/custom_shaders/`. The matrix watches that directory, registers `custom_shader:<name>`, and hot-reloads changed source.

## Minimal contract

Write a regular Shadertoy image shader with:

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // ...
}
```

The renderer provides the normal image built-ins (`iResolution`, `iTime`, `iTimeDelta`, `iFrame`, `iMouse`, `iDate`). Local `.frag` scenes also receive the live music texture as `iChannel0`; simply ignore it for non-musical scenic shaders.

## Music contract

`iChannel0` follows the Shadertoy music texture convention and is 512×2:

- row 0 is normalized spectrum from bass to treble;
- row 1 is waveform encoded from 0..1.

Prefer the renderer helpers instead of hard-coding texture-row coordinates:

```glsl
float energy = sampleAudioSpectrum(iChannel0, 0.18);
float wave = sampleAudioWaveform(iChannel0, uv.x);
```

For semantic reactions, use the named host analysis values directly:

- `iAudioLoudness`, `iAudioBass`, `iAudioMid`, `iAudioTreble`
- `iAudioOnset`, `iAudioKick`, `iAudioSnare`, `iAudioHihat`
- `iAudioBpm`, `iAudioBeatPhase`, `iAudioBeatConfidence`, `iAudioBeatStrength`
- `iAudioStereoWidth`, `iAudioStereoBalance`, `iAudioStereoCorrelation`, `iAudioEnergyTrend`
- `iAudioDrop`, `iAudioSectionChange`, `iAudioSpectralCentroid`, `iAudioSpectralFlux`
- `iAudioAvailable`, `iAudioSilence`, `iAudioSampleRate`

These are stable normalized signals where applicable. Use spectrum/waveform for fine shape and the semantic values for choreography. A good music shader should still produce an attractive idle animation when `iAudioAvailable < 0.5` or `iAudioSilence > 0.5`.

Imported Shadertoy shaders with `music`, `musicstream`, `mic`, or `audio` inputs are connected to the same host music texture automatically.

## Metadata for Automatic Mode

Put an optional JSON block near the beginning of the file. This lets Automatic Mode and Music Director understand an agent-authored shader without any C++ edits.

```glsl
/* led-matrix-shader
{
  "family": "tunnel",
  "tags": ["music", "neon", "depth", "audio-reactive"],
  "intensity": 0.82,
  "motion": 0.90,
  "music_affinity": 1.0,
  "performance_cost": 0.45,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/
```

All normalized numeric values are 0..1. Missing fields use conservative defaults. A shader without this metadata is still manually selectable, but is **not** eligible for Automatic Mode by default. Set `audio_reactive` only when the rendered image actually reacts to music; it declares the audio capability used by runtime selection.

The optional `showcase` tag has selection meaning: Automatic Director and Music Director give a modest quality preference to scenes carrying it. **Only add `showcase` after rendering and visually inspecting the full idle/early/kick/groove/late review set at matrix resolution.** It is a curation signal, not a generic synonym for "shader". Repo shaders automatically also receive `shader`, `shadertoy`, and `gpu-rendered` tags at runtime.

## Low-resolution visual rules

The physical display is much less forgiving than a desktop Shadertoy tab. Prefer large forms, strong silhouettes, broad glows, and motion that survives downsampling. Avoid relying on sub-pixel lines, text, dense high-frequency noise, or tiny stars as the main visual. Anti-alias analytically with `smoothstep`/distance fields rather than many samples.

Keep loops bounded and small. The desktop renderer is GPU-backed, but the scene still has to render at the matrix cadence and read back a frame. Avoid pathological raymarch counts and expensive nested loops unless the visual gain is obvious at matrix resolution.

Music should modulate structure instead of flashing the entire framebuffer. Good mappings include bass → scale/depth, onset/kick → short geometric impulses, mids → deformation, treble/hihat → fine accents, beat phase → continuous cyclic motion, and drop/section change → rare palette or composition changes.

For radial visuals, remember that `atan(y, x)` has a coordinate seam at `-PI/+PI`. Do not feed `fract(angle / TAU + 0.5)` straight into a non-periodic spectrum texture when that sample changes geometry: FFT bin 0 and bin 511 are unrelated and will split a supposedly closed ring. A seam-safe mirrored mapping such as `0.5 - 0.5 * cos(angle)` or an explicit crossfade around the wrap keeps closed geometry closed. Likewise, depth layers driven by `fract()` should fade to zero at **both** ends of the wrapped interval before reappearing.

## Agent workflow

1. Start from one of `shaders/*.frag` or a tiny `mainImage`.
2. Add the metadata block first so intent is explicit.
3. Make the no-audio frame attractive before adding reactions.
4. Add semantic music choreography, then optional spectrum/waveform detail.
5. Run `./scripts/preview_shadertoy_shader.sh plugins/Shadertoy/shaders/<name>.frag`. It builds the exact LED Matrix consumer preview tool if needed and renders a five-frame review set at matrix-like 128×64 resolution: idle/no-audio, early motion, a strong synthetic kick, groove, and a later frame. Use `shadertoy_shader_preview` directly only when you need a specific size/frame count.
6. Inspect the actual generated PNGs, not only compiler success. When working through Laptop MCP, use `sandbox_read_image` on the generated files so the agent literally sees the rendered pixels. Check for clipping, empty frames, unreadable detail, temporal collapse, and excessive full-screen flashes. For looping/depth/radial shaders, also inspect consecutive frames across the expected wrap point.
7. `shadertoy_shader_preview` reports normalized `temporal_mean` and `temporal_max`. An optional final numeric argument turns `temporal_max` into a hard regression limit; this is useful for seam-prone loops. The built-in spectrum tunnel uses a long no-audio temporal smoke test for exactly this reason.
8. For repo shaders, no registry edit is needed: committing the `.frag` file is enough for packaging, Automatic Mode metadata, and scene discovery.

The goal is one self-contained shader file with obvious intent, no bespoke C++ glue, deterministic previewability, and graceful behavior both with and without live music.
