/* led-matrix-shader
{
  "family": "celestial",
  "tags": ["music", "ambient", "scenic", "organic", "bloom", "flower", "audio-reactive", "soft", "showcase", "style:chill", "style:acoustic"],
  "intensity": 0.66,
  "motion": 0.55,
  "music_affinity": 0.96,
  "performance_cost": 0.36,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

// A luminous dahlia seen from above, floating in a violet nebula. Four
// whorls of solid petals turn slowly against each other; bass opens the
// flower, each outer petal leans out with its own spectrum band, onsets
// brighten the petal tips and the golden centre breathes with the beat.
// Pollen drifts out of the centre.

#define TAU 6.28318530718

float hash21(vec2 p) {
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 31.23);
    return fract(p.x * p.y);
}

// One whorl of n petals. Returns (coverage, along-petal 0..1, across -1..1).
// The petal is a pointed teardrop of length len starting at radius r0.
vec3 whorl(vec2 uv, float n, float rot, float r0, float len, float width, float bandGain, float live, out float petalId) {
    float r = length(uv);
    float a = atan(uv.y, uv.x) + rot;
    float sector = TAU / n;
    float k = floor(a / sector + 0.5);
    float la = a - k * sector;
    petalId = mod(k, n);
    // Each petal reaches out with a band of the spectrum (mirrored, seam-safe).
    float u = 0.5 - 0.5 * cos((petalId + 0.5) / n * TAU);
    float l = len * (1.0 + bandGain * live * sampleAudioSpectrum(iChannel0, 0.04 + 0.45 * u));
    vec2 q = vec2(r * cos(la), r * sin(la));
    float t = (q.x - r0) / l;                       // 0 at base, 1 at tip
    float halfW = width * l * sin(clamp(t, 0.0, 1.0) * 3.14159) * mix(1.0, 0.65, t);
    float d = abs(q.y) - halfW;
    float aa = 1.2 / iResolution.y;
    float cover = smoothstep(aa, -aa, d) * step(0.0, t) * smoothstep(1.0, 0.97, t);
    return vec3(cover, clamp(t, 0.0, 1.0), q.y / max(halfW, 1e-4));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float r = length(uv);
    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    float bass = mix(0.22 + 0.10 * sin(iTime * 0.5), iAudioBass, live);
    float mid = mix(0.18, iAudioMid, live);
    float onset = mix(0.0, iAudioOnset, live);
    float beat = mix(0.15 + 0.10 * sin(iTime * 1.1), iAudioBeatStrength, live);
    float drop = live * iAudioDrop;
    float beatPhase = mix(fract(iTime * 0.9), iAudioBeatPhase, live);

    // Background: deep violet nebula, lighter toward the flower.
    vec3 col = mix(vec3(0.020, 0.010, 0.050), vec3(0.090, 0.030, 0.120), exp(-3.0 * r * r));
    float neb = 0.5 + 0.5 * sin(uv.x * 5.0 + iTime * 0.07) * sin(uv.y * 4.0 - iTime * 0.05 + 1.3);
    col += vec3(0.05, 0.02, 0.07) * neb * smoothstep(0.25, 0.7, r);

    // Faint stars in the corners.
    vec2 g = uv * 18.0;
    vec2 cell = floor(g);
    float seed = hash21(cell);
    float star = step(0.93, seed) * smoothstep(0.30, 0.05, length(fract(g) - 0.5));
    col += vec3(0.7, 0.75, 1.0) * star * smoothstep(0.38, 0.6, r) * (0.25 + 0.25 * sin(iTime * (0.5 + seed) + seed * 20.0));

    float open = 0.86 + 0.16 * bass + 0.05 * drop;   // bass opens the flower
    float spin = iTime * 0.035 + 0.15 * mid;   // audio offsets, never scales, the clock

    // Petal layers, outer first. Colours run from deep magenta outside to
    // peach and cream inside.
    vec3 baseCols[4] = vec3[4](vec3(0.55, 0.06, 0.40), vec3(0.80, 0.18, 0.45), vec3(0.95, 0.42, 0.42), vec3(1.00, 0.70, 0.48));
    vec3 tipCols[4]  = vec3[4](vec3(0.95, 0.45, 1.00), vec3(1.00, 0.55, 0.80), vec3(1.00, 0.78, 0.65), vec3(1.00, 0.92, 0.75));
    float counts[4] = float[4](11.0, 9.0, 8.0, 7.0);
    float lens[4]   = float[4](0.30, 0.25, 0.18, 0.12);
    float r0s[4]    = float[4](0.10, 0.07, 0.05, 0.035);

    float shadow = 0.0;
    for (int i = 0; i < 4; ++i) {
        float fi = float(i);
        float dir = (i % 2 == 0) ? 1.0 : -1.0;
        float rot = dir * spin * (1.0 + 0.3 * fi) + fi * 0.37;
        float id;
        // Each beat ripples outward through the layers: inner whorls open first.
        float stagger = exp(-fract(beatPhase - (3.0 - fi) * 0.12) * 5.0) * beat * 0.06;
        vec3 w = whorl(uv, counts[i], rot, r0s[i], lens[i] * (open + stagger), 0.36, i == 0 ? 0.30 : 0.14, live, id);
        if (w.x <= 0.0) continue;
        float along = w.y;
        float across = w.z;
        // Per-petal variation and a gentle breathing ripple around the whorl.
        float vary = 0.88 + 0.12 * hash21(vec2(id, fi));
        vec3 petal = mix(baseCols[i], tipCols[i], smoothstep(0.1, 0.95, along)) * vary;
        // Cupped shading: darker at the edges, light centre vein.
        petal *= 0.55 + 0.45 * (1.0 - across * across);
        petal += tipCols[i] * 0.25 * exp(-across * across * 40.0) * smoothstep(0.1, 0.6, along);
        // Lit tips on onsets and in the beat.
        petal += tipCols[i] * smoothstep(0.75, 1.0, along) * (0.15 + 0.6 * onset + 0.25 * beat);
        // Shadow cast by the layer above (darkens the base of this petal).
        petal *= 1.0 - 0.45 * shadow;
        col = mix(col, petal, w.x);
        shadow = w.x * smoothstep(0.35, 0.0, along);
    }

    // Golden centre with seed dots in a phyllotaxis spiral.
    float cr = 0.055 + 0.012 * beat + 0.006 * bass;
    float centre = smoothstep(cr + 0.01, cr - 0.005, r);
    vec3 gold = mix(vec3(0.80, 0.45, 0.06), vec3(1.0, 0.85, 0.35), smoothstep(cr, 0.0, r));
    float seeds = 0.0;
    for (int i = 0; i < 13; ++i) {
        float fi = float(i);
        float sr = cr * 0.85 * sqrt((fi + 0.5) / 13.0);
        float sa = fi * 2.39996 + iTime * 0.15;
        seeds += smoothstep(0.011, 0.004, length(uv - sr * vec2(cos(sa), sin(sa))));
    }
    gold = mix(gold * 0.65, vec3(1.0, 0.95, 0.70), clamp(seeds, 0.0, 1.0) * (0.5 + 0.5 * beat));
    col = mix(col, gold, centre);
    col += vec3(1.0, 0.75, 0.35) * exp(-r * 22.0) * (0.10 + 0.35 * beat);

    // Pollen drifting outward from the centre.
    for (int i = 0; i < 12; ++i) {
        float fi = float(i);
        float t = fract(iTime * (0.05 + 0.03 * hash21(vec2(fi, 1.0))) + hash21(vec2(fi, 2.0)));
        float ang = hash21(vec2(fi, 4.0)) * TAU + t * 0.8;
        vec2 p = vec2(cos(ang), sin(ang)) * (0.06 + t * 0.55);
        float m = smoothstep(0.012, 0.003, length(uv - p)) * sin(t * 3.14159) * smoothstep(0.12, 0.30, length(p));
        col += vec3(1.0, 0.85, 0.5) * m * (0.45 + 0.6 * onset);
    }

    col = 1.0 - exp(-col * 1.35);
    fragColor = vec4(col, 1.0);
}
