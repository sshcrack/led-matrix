# Scene authoring guide

The decisions a new or changed matrix scene needs, and where each system fits.
Work through the sections in order, then the [checklist](#checklist) at the end.

- The rules every scene follows are in [AGENTS.md → Design constraints](../AGENTS.md#design-constraints-read-before-proposing-features).
- Code mechanics (plugin class, CMake, Runtime Inputs, migrating simulation state) are in [Plugin development](PLUGIN_DEVELOPMENT.md).
- The execution model (Pi, preview, desktop worker, contracts) is in [Scene execution](SCENE_EXECUTION.md).
- Shader scenes (`.frag` files) have their own [Shader authoring](../plugins/Shadertoy/SHADER_AUTHORING.md) guide; the descriptor, startup and review sections below still apply.

## 1. Where the scene lives

- Pick the plugin by subject (`HobbyScenes` for native scenery and hobbies, `AudioVisualizer` for music scenes, `GenerativeScenes` for simulations, `Shadertoy` for GLSL). Look at a close sibling first; most systems below have an adopter to copy.
- Portable scene code goes in `plugins/<Name>/scenes/` (newer plugins) or `plugins/<Name>/matrix/scenes/` (HobbyScenes and older plugins). Everything listed before `DESKTOP` in `register_plugin()` is also compiled into the desktop scene worker, so write the renderer once.
- The plugin exports `create<Name>`/`destroy<Name>` through `REGISTER_PLUGIN(Name, Type)` (`shared/common/include/shared/common/plugin_macros.h`); `Name` must match the plugin name exactly.
- Properties: `MAKE_PROPERTY` / `MAKE_PROPERTY_MINMAX` (`shared/matrix/include/shared/matrix/plugin/PropertyMacros.h`), each added once in `register_properties()`. `Scene::to_json()` is the one property payload; it reaches presets, previews and the worker.
- Host config: `BasicPlugin::host_config()` is null while the host builds its config, and scenes for saved presets are created then. Tolerate null, or receive the values you need from the plugin.
- Time: animate from `frame_context().delta_seconds` / `elapsed_seconds`, never `steady_clock`. Seed randomness in `set_random_seed()` and return true from `supports_virtual_time()` so `preview_gen --virtual-time-only` is deterministic.
- Changing an existing scene: keep its id (`get_name()`) so saved presets and director preferences still match, even when the look is rebuilt (`boids` is now the starling murmuration). Rename properties with `legacy_name()` instead of dropping them.

## 2. Descriptor: how Automatic Mode sees the scene

Override `get_descriptor()` (`shared/matrix/include/shared/matrix/scene_descriptor.h`), starting from `Scene::get_descriptor()`:

- `automatic_eligible`: `Scene::get_descriptor()` sets it to **false** (the struct default is true, the base method overrides it). Set `d.automatic_eligible = true` for ambient scenes. Scenes with readable text, games and interactive scenes keep false.
- `family`, `intensity`, `motion`, `music_affinity`, `performance_cost` (all 0..1): the directors rank by them. `performance_cost` is a hint only; placement follows measured Pi cost.
- `tags`, with these carrying selection meaning:
  - `scenery`: an ambient place or object.
  - `fits:<id>`: the moments the scene suits, ids from `Environment::fit_id` (`dawn|day|dusk|night`, `clear|cloudy|fog|rain|snow|storm`, `spring|summer|autumn|winter`).
  - `occasion:<id>`: holiday-only; Automatic Mode skips it unless that occasion is active (`AutomaticDirectorRanking.cpp`).
  - `music-showcase`: the preferred, longer-dwelling tier while music plays. It **requires** at least one `style:<chill|edm|party|rock|hiphop|acoustic>` tag (section 6).
  - `audio-reactive` is added by the base descriptor when `get_capabilities().supports_audio` is set; `environment-adaptive` and `music-layer` are added by the helpers in sections 4 and 5.

Tests that read the `preview_gen --dump-manifest` output and enforce this:

| Test | Fails when |
|---|---|
| `tests/scenery_eligibility_coverage.py` | a scene tagged `scenery` or `fits:*` is not eligible and not on its `OPT_OUT` list (with a reason) |
| `tests/music_showcase_coverage.py` | a `music-showcase` scene has no `style:` tag, or a style falls below its minimum (3; hiphop 10, rock 5, party 5) |
| `tests/automatic_director_smoke.cpp` | director ranking, modes and fits regress |

## 3. Variants

A variant is a curated look: `Scenes::SceneVariant{id, label, description, properties, tags, intensity, motion, music_affinity, performance_cost, automatic_default}`. `properties` holds only overrides of the scene defaults; the optional profile numbers replace the scene's for that look.

- Add variants when the scene has distinct looks worth picking in presets (calm/vivid, music, palettes). Example: `ReactionDiffusionScene::get_descriptor()`.
- `automatic_default = false`, or `Scenes::set_off_by_default(d, {"id", …})`, keeps a look out of Automatic Mode unless the user opts it in: use it for extreme, heavy or near-duplicate looks.
- How Automatic Mode plays them (`CanvasCoordinator::ensure_automatic_catalog`): a scene without variants plays its default properties; a scene with variants plays only its variants, unless it carries `environment-adaptive`, which adds the real-world-following default look ("Follows the real world" in the web UI) next to the variants.
- Environment scenes get their fixed looks (`night`, `rain`, `winter`, …) from `SceneEnvironment::add_variants(d, extra)` (section 4); these are off in Automatic Mode by default and the call adds `environment-adaptive`.
- Render one look with `preview_gen --scenes name/variant` (output `name--variant`); list all with `preview_gen --dump-manifest --manifest-out /tmp/scenes.json`.

## 4. Real-world environment (time of day, weather, season)

Use it for outdoor scenery, so the scene matches the room's window. Headers: `shared/matrix/include/shared/matrix/environment/look.h` and `paint.h`. The daemon side (location, Open-Meteo, `config.json` → `environment`, REST, Integrations UI) is in AGENTS.md → Real-world environment.

- The scene owns an `Environment::SceneEnvironment(Defaults, Adapt)`, adds `properties()` in `register_properties()` (time_of_day / weather / season enums, group "Environment"), calls `update(raw_dt)` once per frame with the unscaled delta and paints with the returned `Look`: smoothed numbers (`daylight`, `night`, `rain`, `snow_cover`, `wetness`, season weights, …) and a palette (`sky()`, `lit()`, `subject()`, `foliage()`, `grass()`, `ground()`, `lamp()`).
- Each aspect resolves independently: scene property > global override > real world > scene `Defaults`.
- `Adapt` turns aspects off for scenes whose identity is fixed (a campfire stays night, skiing stays winter); `weather_allowed` drops weather the scene cannot show (an aquarium and snow). `season_variants` / `weather_variants` limit which fixed looks are offered as variants while the real value still shows.
- Call `add_variants(descriptor)` in `get_descriptor()` (section 3).
- `paint.h` painters work on any float RGB buffer (`Surface`, unit 1 or 255): `paint_sky`, `paint_stars`, `paint_sun`, `paint_moon` (with phase), `paint_clouds`, `paint_fog`, `paint_tree`, `Precipitation`, `Lightning`, `SeasonalDrift` (leaves, petals, fireflies), `paint_window`, `paint_string_lights`.
- HobbyScenes: `HobbySceneBase::use_environment(defaults, adapt)` in the constructor wires all of this; then use `env()`, `lit()`, `plant()`, `meadow()`, `terrain()`, `paint_sky_layers()`, `paint_weather()` and `environment_changed(key)` (`plugins/HobbyScenes/matrix/scenes/HobbyScene.h`).
- Cached backdrops (harbour, viaduct, New Year's Eve) share `plugins/HobbyScenes/matrix/scenes/Backdrop.h`: design-space drawing (`Artwork`), open-sky/water masks (`unchanged()`), clouds behind the landscape (`paint_clouds_behind()`) and light on masked water (`MaskedLight`: streaks, sub-pixel points, the sun/moon path). Repaint only when `environment_changed()` says so.
- Holidays: an entry in `registry()` (`shared/matrix/src/shared/matrix/environment/occasions.cpp`) lists the occasion in the UI; a scene decorates with `look.occasion("id")` (example: `season_cottage`). Holiday-only scenes add `occasion:<id>`.
- Check every look you touch: `preview_gen --scenes name/night,name/rain,name/snow,name/winter`. `tests/environment_smoke.cpp` covers astronomy, parsing and layering.

## 5. Music layer

Spectrum bars (the `audio_spectrum` look) rise in the open sky behind a scenery scene while music plays, so a calm scene can stay up through a music session. Header: `shared/matrix/include/shared/matrix/music_layer.h` (`MusicLayer::Layer`, `Placement`). Adopters: `harbour_dawn` first, then `railway_viaduct`, `allotment`, `desert_dunes` and `boids`; #292 tracks the remaining scenery.

**When to add it:** outdoor scenery with open sky above a horizon. Leave it out of scenes that are already music visuals (`music-showcase`, `AudioVisualizer`) and of scenes without sky (interiors, underwater, close-ups).

How it works:
- HobbyScenes: `use_music_layer()` in the constructor, `music_layer_->describe(d)` in `get_descriptor()` (adds the `music-layer` tag unless switched off), `paint_music_layer(mask, placement)` right after the cached sky, so clouds and the landscape stay in front. Other plugins own a `MusicLayer::Layer`, add `property()`, call `describe()`, then `update(raw_dt)` and `paint(surface, mask, placement)` every frame.
- The scene passes an open-sky mask (one byte per pixel, non-zero where bars may show; `Backdrop.h` builds one) and a `Placement` (`base` = horizon row, `max_height`, `brightness`: dim it under a bright day sky).
- Shown while Spotify plays or live audio is non-silent; without audio capture the bars swell gently. Off switches: the per-scene `music_layer` property (`OFF` also drops the `music-layer` tag) and the global `config.json` → `music_layer` (`GET/POST /music_layer`, Integrations page), published as the `music.layer` Runtime Input so the desktop worker sees it.
- Automatic Mode: while the layer is live (`MusicLayer::live()`), tagged scenery counts as a gentle music look (`with_music_layer()` in the director), still ranked below `music-showcase` scenes.

## 6. Music reactivity (music scenes)

- Audio comes from `AudioState::snapshot()` (`shared/matrix/include/shared/matrix/audio_state.h`), mirrored to the worker. Declare `caps.supports_audio = true` in `get_capabilities()` when audio enhances the scene, `requires_audio` only when it is unusable without.
- Helpers: `AudioKit` (`shared/matrix/include/shared/matrix/audio_kit.h`: `SpectrumFollower`, `EventTracker`, `Envelope`, `HueCycle`, `Buffer`), `AmbientMusicScene` (`plugins/AudioVisualizer/scenes/kit/AmbientMusicScene.h`) for music scenes set in a place, and `AudioHobbyScene`/`AudioPulse` in HobbyScenes (fades to calm without music). Beat detection notes: `plugins/AudioVisualizer/BEAT_DETECTION.md`.
- Hide the analysis inside a recognisable place (kelp heights as the spectrum, streetlights on the beat) rather than raw meters.
- Style matching: while live music plays, both directors prefer `music-showcase` scenes (longer dwell) and match `style:` tags against the current music style (`shared/matrix/music_style.h`, `style_fit()`). The style fuses genre metadata (`RuntimeInputIds::MusicGenre`, published by SpotifyScenes `genre_lookup.cpp`: Spotify artist genres first, then Deezer, then the optional GetSongBPM key; cached in `cache/genres.json`) with a weak audio estimate. Tag only the styles the scene really suits.
- The MusicDirector scene keeps its own candidate pools (`MusicDirectorScene::preferred()` in `plugins/AudioVisualizer/scenes/MusicDirectorScene.cpp`); add a new showcase scene there when it belongs in a musical state.
- Coverage minimums are floors that keep a style from going empty, not a reason to keep a weak scene. Hiphop sits exactly at its minimum of 10 (2026-11-27): retiring or retagging a hiphop scene needs a replacement in the same PR. The thresholds stay as they are.
- Check with real music: `audio_analysis_dump <song.wav> <out.audr>` records the analyzer output; `render_scene_video.sh --replay out.audr` drives the scene with it; `director_music_sim <song.audr> [--desktop] [--spotify] [--genre "a,b"]` replays it through the real director.

## 7. Startup look and long runs

A scene must look complete within about 3 s of activation and never freeze, collapse or sit near-black during a long run. This was the most common fault in the 2026-11-27 audit (#300): `julia_set` collapsed into a still blob, `game_of_life`/`reaction_diffusion` started empty for 30–40 s, `wave_pattern` looks were near-black, `boids` started with an empty sky.

- Simulations pre-warm (run steps in `initialize()`) or seed densely, and reseed before they settle into a still state.
- Scenes with arrivals (flocks, trains, fireworks) start mid-performance: the first train runs as `railway_viaduct` opens, `boids` opens on a flock already wheeling.
- Iterated or zooming effects stay bounded and keep moving.

Check every variant, at the start and over a long run (15 fps; metrics per output, `name--variant` for variants):

```bash
B=emulator_build/install/bin
# first 3 s: expect a plausible mean_luma_average and a lit_fraction_min well above 0
$B/preview_gen --scenes my_scene,my_scene/night --frames 45 --virtual-time-only --metrics-out /tmp/start.json --output /tmp/start
# 60 s+: temporal_change_average must stay > 0; look at the late frames of the GIF too
$B/preview_gen --scenes my_scene,my_scene/night --frames 900 --virtual-time-only --metrics-out /tmp/long.json --output /tmp/long
```

`--virtual-time-only` silently skips scenes without `supports_virtual_time()`; drop the flag for them (they render in real time). Keys: `mean_luma_average`, `lit_fraction_average/min/max`, `temporal_change_average/max`. Night looks are legitimately dark; compare against a sibling night scene. #309 tracks a CI check for this rule.

## 8. Pi cost

The real device is a Raspberry Pi 3 Model B (`ssh ledmat`, Cortex-A53 at 1.2 GHz, no GPU). x64 gains overstate Pi gains: removing libm calls made scenes 1.3–2.6× cheaper on x64 but only 3–20 % cheaper on the Pi, where per-pixel float math and memory traffic dominate. Real savings come from doing less per-pixel work.

- `Scene::render_quality_scale()` (0.5..1) is the one quality knob: 1.0 on the desktop worker and in previews, adaptive on the Pi; `set_render_quality_hint()` sets a Pi-only start value. Map it to cheaper optional work: particle counts, or smooth layers drawn through `AudioKit::Buffer::add_field()` with `AudioKit::field_step(quality)` (per pixel at full quality, an interpolated lattice below).
- Cache static per-pixel geometry (backdrops, distance maps) instead of recomputing it each frame; allocate nothing per frame.
- Prefer `AudioKit::len` over `std::hypot` (a libm call on the Pi).
- Check the reduced look: `preview_gen --quality 0.6` / `render_scene_video.sh --quality 0.6`.
- Measure on the Pi with a Scene Lab A/B: `POST /scene_lab/start {"scene":…, "fps":60}`, then `/diagnostics` → `renderer.scene_performance[<scene>]` (`render_ms_p95`); alternate old and new builds at least twice, single runs are noisy.

Desktop offload: the Pi offloads a scene to the desktop scene worker when its p95 exceeds 72 % of the frame budget or its quality is below 1; the worker renders full quality. Portable scenes need nothing for this. Bump `Scene::remote_contract_version()` (default 1) when properties, variants or runtime state change incompatibly (`boids` went to 2 when it was rebuilt); see [Scene execution → Remote data contracts](SCENE_EXECUTION.md#remote-data-contracts).

## 9. Previews and verification

- `get_preview_spec()` declares preview eligibility and fixtures. Non-desktop scenes are previewable by default; desktop-dependent scenes opt in. A scene can request named fixture inputs and preview-only property overrides:

  ```cpp
  [[nodiscard]] Previews::SceneSpec get_preview_spec() const override {
      auto spec = Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
      spec.set_property("audio_reactive", true);
      return spec;
  }
  ```

- Plugins own fixtures by overriding `BasicPlugin::create_preview_data_providers()` and returning `Previews::DataProvider` adapters that update the same state the production source updates. `preview_gen` resolves the scene's input IDs and calls `begin()`, `update(SceneFrameContext)` per frame, then `end()`; registry validation rejects duplicate/missing provider IDs and invalid property overrides. Current providers: `audio` (AudioVisualizer) and `spotify.playback` (SpotifyScenes: deterministic playback and synthetic artwork, no credentials). Tune them with `--preview-option <provider>:<key>=<json>`, e.g. `audio:bpm=132`, `spotify.playback:duration_ms=30000`, or custom artwork and track changes with `'spotify.playback:covers=["a.png","b.png"]'` plus `spotify.playback:track_seconds=6` (`--audio-bpm` / `--audio-profile` remain compatibility aliases). Only scenes whose spec stays disabled need `scripts/capture_desktop_preview.sh`.
- Network-dependent scenes (`weather`, `image_scene`) use offline fixtures only with `LED_MATRIX_PREVIEW_OFFLINE=1`.
- Render: `preview_gen --scenes a,a/variant [--quality 0.6] [--virtual-time-only --strict] [--metrics-out f.json]` (emulator preset build; all flags are listed at the top of `src_preview_gen/main.cpp`).
- PR media: `scripts/render_scene_video.sh --scenes a,b [--seconds n] [--replay song.audr] [--quality 0.6]` renders MP4s with an LED-panel look; `scripts/pr_media.sh [--comment] <files>` uploads them to the `pr-media` prerelease and prints markdown (videos get a linked GIF preview, since GitHub only plays inline video uploaded through the browser). Publish silent videos; keep `--audio` versions with copyrighted music local.
- Published scene GIFs (`scene_previews/`, the `scene-previews` prerelease): generate with `scripts/generate_scene_previews.sh --scenes …`. `scripts/publish_scene_previews.sh` deletes every release asset missing locally, so right before publishing run a full fresh `rm -rf scene_previews && scripts/fetch_scene_previews.sh`, copy in only this PR's new GIFs, delete removed scenes' GIFs, publish immediately, and compare local sha256 against the release digests (parallel PRs race).
- CI: `virtual_scene_preview_smoke` and, in sanitizer builds, `sanitizer_scene_sweep` (every previewable scene, 30 frames, `--strict --allow-still`) catch crashes and sanitizer reports, not weak looks.

## 10. Review

Scene PRs get an independent **visual** review by Codex `gpt-6.2-sol` (through `delegate_task`); add a code review only when asked. Keep the brief blind and neutral: what to render and how, nothing about a target score, the round, which looks are new or reworked, or your own ratings. The reviewer scores each scene and variant; fix the findings and run a fresh review until every look scores 8/10 or more. If scores stall over a few rounds, fix the clearest remaining points once and ship; file a polish issue for notable leftovers. The [`led-matrix-ship`](../.agents/skills/led-matrix-ship/SKILL.md) skill carries the PR side.

## 11. Retiring a scene

Retire a scene that stays weak after a rework attempt rather than keeping it for coverage (examples: `d15decad`, `ddf311be`).

- Remove the scene files, its `create_scenes()` registration and `register_plugin()` sources.
- Saved presets that name it must still load: an unknown scene id becomes a `FallbackScene` (`shared/matrix/include/shared/matrix/FallbackScene.h`) and is skipped. Keep any config migration that reads its old properties.
- Remove it from director pools (`MusicDirectorScene::preferred()`), ambiance packs (`shared/matrix/src/shared/matrix/ambiance_packs.cpp`), plugin web templates (`plugins/<Name>/web/`), `tests/visual_regression_baselines.json`, `scripts/generate_scene_previews.sh` and docs (plugin README, README scene lists).
- Re-run the coverage tests: a retired `music-showcase` or `fits:*` scene can drop a style or moment below its floor; replace it in the same PR.
- Delete its GIFs from the scene-previews set when publishing (section 9).
- To rebuild a look under the same id instead, keep the id and bump `remote_contract_version()`.

## Checklist

For every new or changed scene, decide and verify:

- [ ] **Rules:** calm, recognisable, non-interactive, cheap on the Pi; readable text only if it stays manual.
- [ ] **Eligible?** `automatic_eligible = true` for ambient scenes; family, intensity, motion, music affinity, tags.
- [ ] **Fits / occasion?** `fits:*` for the moments it suits; `occasion:<id>` if holiday-only.
- [ ] **Variants?** Curated looks, extreme ones off by default; every variant previewed.
- [ ] **Environment?** Outdoor scenery follows time, weather and season; `Adapt` for fixed identities; `add_variants()`.
- [ ] **Music layer?** Open sky above a horizon and not already a music visual → add it.
- [ ] **Music showcase?** `music-showcase` plus honest `style:*` tags; coverage tests pass.
- [ ] **Quality scale?** Optional work follows `render_quality_scale()`; `--quality 0.6` still looks right; static geometry cached.
- [ ] **Contract?** `remote_contract_version()` bumped for incompatible property/variant/state changes.
- [ ] **Preview spec?** Fixtures declared; deterministic under `--virtual-time-only`.
- [ ] **Startup look?** Complete within 3 s and alive after 60 s+, for every variant (`--metrics-out`).
- [ ] **Media and review:** silent video in the PR, blind visual review at 8+ (or stalled and shipped).
