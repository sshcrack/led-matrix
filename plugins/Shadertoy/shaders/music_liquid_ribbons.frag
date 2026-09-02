/* led-matrix-shader
{
  "family": "ribbons",
  "tags": ["music", "ribbons", "flow", "organic", "audio-reactive", "soft", "showcase"],
  "intensity": 0.66,
  "motion": 0.72,
  "music_affinity": 1.0,
  "performance_cost": 0.31,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

#define TAU 6.28318530718

vec3 ribbonPalette(float t) {
    vec3 a = vec3(0.48, 0.43, 0.54);
    vec3 b = vec3(0.52, 0.50, 0.42);
    vec3 c = vec3(1.0);
    vec3 d = vec3(0.02, 0.18, 0.46);
    return a + b * cos(TAU * (c * t + d));
}

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    vec2 screenUv = fragCoord / iResolution.xy;
    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    float bass = mix(0.18 + 0.035 * sin(iTime * 0.9), iAudioBass, live);
    float mids = mix(0.17 + 0.025 * sin(iTime * 0.73 + 1.4), iAudioMid, live);
    float treble = mix(0.10, iAudioTreble, live);
    float onset = mix(0.08, iAudioOnset, live);
    float beat = mix(0.10, iAudioBeatStrength, live);

    vec3 color = vec3(0.0025, 0.005, 0.014);
    color += vec3(0.006, 0.012, 0.028) * (0.35 + 0.65 * (1.0 - screenUv.y));
    float t = iTime * (0.21 + 0.07 * mids);
    // The physical panel is square. Give the flow enough horizontal phase
    // travel to feel organic without squeezing the ribbon stack into a narrow
    // widescreen band.
    float flowX = uv.x * 1.46;
    float mist = 0.0;

    for (int i = 0; i < 7; ++i) {
        float fi = float(i);
        float lane = (fi - 3.0) * 0.112;
        float samplePos = clamp(0.06 + fi * 0.145, 0.0, 1.0);
        float band = mix(0.13, sampleAudioSpectrum(iChannel0, samplePos), live);
        float wave = mix(0.0, sampleAudioWaveform(iChannel0, fract(flowX * 0.34 + 0.5 + fi * 0.11)), live);

        float phase = t * (1.16 + fi * 0.055) + fi * 1.21;
        float curve = lane
                    + sin(flowX * (2.7 + fi * 0.17) + phase) * (0.075 + 0.044 * bass)
                    + sin(flowX * 5.2 - phase * 0.74 + fi * 0.83) * (0.022 + 0.020 * mids)
                    + wave * (0.004 + 0.012 * treble);
        float d = abs(uv.y - curve);
        float width = 0.0105 + 0.0080 * band + 0.0035 * onset + 0.002 * beat;
        float core = exp(-d * d / max(0.000035, width * width * 0.36));
        float halo = exp(-d / (0.026 + 0.020 * band));
        float soft = exp(-d / (0.080 + 0.025 * bass));

        vec3 tint = ribbonPalette(fi * 0.105 + flowX * 0.075 + iTime * 0.020);
        float depth = 0.75 + 0.25 * sin(fi * 1.9 + iTime * 0.18);
        color += tint * core * depth * (0.30 + 0.50 * band);
        color += tint * halo * (0.018 + 0.035 * bass);
        mist += soft * (0.018 + 0.015 * band);
    }

    // A very soft shared glow makes the seven lines read as one flowing volume
    // on an LED panel instead of isolated one-pixel traces.
    color += ribbonPalette(0.54 + iTime * 0.012) * mist * (0.25 + 0.16 * bass);

    // Sparse, dim motes provide depth without competing with the ribbons.
    vec2 cell = floor((uv + vec2(1.05, 0.55)) * vec2(19.0, 14.0));
    float moteSeed = hash21(cell);
    vec2 local = fract((uv + vec2(1.05, 0.55)) * vec2(19.0, 14.0)) - 0.5;
    float mote = step(0.965, moteSeed) * exp(-34.0 * dot(local, local));
    mote *= 0.35 + 0.65 * sin(iTime * 0.45 + moteSeed * TAU) * sin(iTime * 0.45 + moteSeed * TAU);
    color += vec3(0.24, 0.46, 0.72) * mote * (0.035 + 0.08 * treble);

    float vignette = 0.72 + 0.28 * exp(-0.95 * dot(uv, uv));
    color *= vignette;
    color *= 0.94 + 0.08 * beat;
    color = 1.0 - exp(-color * 1.55);
    fragColor = vec4(color, 1.0);
}
