/* led-matrix-shader
{
  "family": "celestial",
  "tags": ["music", "ambient", "scenic", "organic", "bloom", "audio-reactive", "soft", "showcase"],
  "intensity": 0.74,
  "motion": 0.68,
  "music_affinity": 0.96,
  "performance_cost": 0.36,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

#define TAU 6.28318530718
#define PI 3.14159265359

vec3 bloomPalette(float t) {
    vec3 a = vec3(0.48, 0.48, 0.54);
    vec3 b = vec3(0.52, 0.48, 0.46);
    vec3 c = vec3(1.0);
    vec3 d = vec3(0.00, 0.17, 0.39);
    return a + b * cos(TAU * (c * t + d));
}

float hash21(vec2 p) {
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 31.23);
    return fract(p.x * p.y);
}

float closedSpectrum(float angle, float live) {
    float u = 0.5 - 0.5 * cos(angle);
    return mix(0.13, 0.72 * sampleAudioSpectrum(iChannel0, u)
                    + 0.28 * sampleAudioSpectrum(iChannel0, 1.0 - u), live);
}

float lineGlow(float d, float width) {
    float x = d / max(width, 0.0001);
    return exp(-x * x * 1.7);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float r = length(uv);
    float a = atan(uv.y, uv.x);

    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    float bass = mix(0.19 + 0.040 * sin(iTime * 0.8), iAudioBass, live);
    float mid = mix(0.16 + 0.030 * sin(iTime * 0.67 + 1.1), iAudioMid, live);
    float treble = mix(0.10, iAudioTreble, live);
    float beat = mix(0.12, iAudioBeatStrength, live);
    float onset = mix(0.08, iAudioOnset, live);
    float spectrum = closedSpectrum(a, live);

    float t = iTime * (0.12 + 0.045 * mid);
    float spin = a + t * 1.8;

    // Primary seven-petal bloom: large enough to survive LED downsampling while
    // retaining a crisp, animated silhouette.
    float petals = 0.5 + 0.5 * cos(spin * 7.0 + 0.45 * sin(a * 3.0 - t * 2.1));
    petals = pow(petals, 1.38);
    float breath = 0.335 + 0.055 * bass + 0.018 * sin(iTime * 0.73) + 0.022 * beat;
    float boundary = breath * (0.80 + 0.34 * petals + 0.075 * spectrum);
    float flowerD = abs(r - boundary);
    float flowerCore = lineGlow(flowerD, 0.010 + 0.0035 * spectrum + 0.0025 * onset);
    float flowerHalo = exp(-flowerD / 0.052);

    // A counter-rotating ghost bloom gives the shape depth and keeps the scene
    // visually rich between beats without simply increasing global brightness.
    float ghostPetals = 0.5 + 0.5 * cos((a - t * 1.23) * 5.0 + 0.55 * sin(a * 4.0 + t));
    float ghostBoundary = 0.275 + 0.054 * ghostPetals + 0.024 * mid + 0.015 * spectrum;
    float ghostD = abs(r - ghostBoundary);
    float ghost = lineGlow(ghostD, 0.009);
    float ghostHalo = exp(-ghostD / 0.042);

    float innerPetals = 0.5 + 0.5 * cos((a - t * 1.35) * 5.0 + sin(a * 4.0 + t));
    float innerBoundary = 0.170 + 0.044 * innerPetals + 0.022 * bass;
    float innerD = abs(r - innerBoundary);
    float inner = lineGlow(innerD, 0.0085 + 0.002 * onset);
    float innerHalo = exp(-innerD / 0.034);

    vec3 color = vec3(0.0015, 0.003, 0.012);
    vec3 flowerTint = bloomPalette(0.08 * iTime + a / TAU + 0.12 * spectrum);
    vec3 ghostTint = bloomPalette(0.34 - a / TAU + iTime * 0.018);
    vec3 innerTint = bloomPalette(0.58 - a / TAU + iTime * 0.025);

    // Translucent interior light gives the bloom volume without flattening its
    // distance-field outlines. The swirl is spatial, so beats do not flash the
    // entire interior uniformly.
    float flowerInterior = 1.0 - smoothstep(max(0.08, boundary - 0.095), boundary, r);
    float innerCutout = smoothstep(0.07, 0.18, r);
    float interiorSwirl = 0.52 + 0.48 * sin(a * 7.0 - t * 2.3 + r * 17.0 + spectrum * 1.2);
    interiorSwirl *= interiorSwirl;
    color += mix(innerTint, flowerTint, 0.55 + 0.35 * interiorSwirl)
           * flowerInterior * innerCutout * (0.018 + 0.032 * interiorSwirl + 0.018 * bass);

    color += flowerTint * flowerCore * (0.48 + 0.50 * spectrum + 0.22 * beat);
    color += flowerTint * flowerHalo * (0.035 + 0.045 * bass);
    color += ghostTint * ghost * (0.18 + 0.24 * mid + 0.10 * beat);
    color += ghostTint * ghostHalo * (0.014 + 0.018 * bass);
    color += innerTint * inner * (0.32 + 0.38 * mid + 0.20 * onset);
    color += innerTint * innerHalo * (0.024 + 0.026 * bass);

    // Slow orbital veils frame the bloom and use the wide matrix area.
    for (int i = 0; i < 4; ++i) {
        float fi = float(i);
        float phase = t * (0.66 + fi * 0.16) + fi * 1.73;
        float veilRadius = 0.445 + fi * 0.072
                         + 0.022 * sin(a * (2.0 + fi) + phase)
                         + 0.010 * spectrum;
        float veilD = abs(r - veilRadius);
        float veil = lineGlow(veilD, 0.0065 + fi * 0.0008);
        float arc = pow(0.5 + 0.5 * cos(a * (2.0 + fi) + phase), 4.0);
        vec3 veilTint = bloomPalette(0.20 + fi * 0.17 + iTime * 0.014);
        color += veilTint * veil * arc * (0.075 + 0.060 * mid + 0.040 * beat);
    }

    float center = exp(-11.0 * r * r);
    float centerSpark = exp(-40.0 * r * r);
    color += bloomPalette(0.82 + iTime * 0.018) * center * (0.030 + 0.082 * bass);
    color += vec3(0.80, 0.92, 1.0) * centerSpark * (0.045 + 0.14 * beat);

    // Sparse stars stay subtle at idle and wake up with high-frequency detail.
    vec2 drift = vec2(iTime * 0.003, -iTime * 0.0018);
    vec2 gridUv = (uv + vec2(1.10, 0.56) + drift) * vec2(22.0, 15.0);
    vec2 cell = floor(gridUv);
    vec2 local = fract(gridUv) - 0.5;
    float seed = hash21(cell);
    float star = step(0.958, seed) * exp(-40.0 * dot(local, local));
    float twinkle = 0.30 + 0.70 * sin(iTime * (0.36 + seed * 0.35) + seed * TAU);
    twinkle *= twinkle;
    float outerMask = smoothstep(0.30, 0.62, r);
    color += bloomPalette(seed * 0.7 + 0.18) * star * twinkle * outerMask * (0.045 + 0.10 * treble);

    // A restrained nebular field prevents empty black corners but cannot become
    // a full-screen beat flash.
    float nebula = exp(-2.35 * dot(uv * vec2(0.70, 1.0), uv * vec2(0.70, 1.0)));
    nebula *= 0.55 + 0.45 * sin(a * 2.0 - t * 1.2 + r * 7.0);
    color += bloomPalette(0.48 + 0.06 * sin(t)) * nebula * (0.012 + 0.014 * bass);

    color *= 0.95 + 0.065 * beat;
    color = 1.0 - exp(-color * 1.62);
    fragColor = vec4(color, 1.0);
}
