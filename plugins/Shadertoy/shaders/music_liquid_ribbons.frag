/* led-matrix-shader
{
  "family": "ribbons",
  "tags": ["music", "ribbons", "flow", "organic", "audio-reactive", "soft"],
  "intensity": 0.62,
  "motion": 0.72,
  "music_affinity": 1.0,
  "performance_cost": 0.30,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

#define TAU 6.28318530718

vec3 ribbonPalette(float t) {
    vec3 a = vec3(0.46, 0.43, 0.52);
    vec3 b = vec3(0.50, 0.48, 0.42);
    vec3 c = vec3(1.0);
    vec3 d = vec3(0.02, 0.18, 0.46);
    return a + b * cos(TAU * (c * t + d));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float audioLive = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    float bass = mix(0.16 + 0.04 * sin(iTime), iAudioBass, audioLive);
    float mids = mix(0.15, iAudioMid, audioLive);
    float treble = mix(0.10, iAudioTreble, audioLive);
    float onset = mix(0.08, iAudioOnset, audioLive);

    vec3 color = vec3(0.003, 0.006, 0.014);
    float t = iTime * (0.24 + 0.10 * mids);

    for (int i = 0; i < 5; ++i) {
        float fi = float(i);
        float lane = (fi - 2.0) * 0.105;
        float samplePos = clamp(0.10 + fi * 0.18, 0.0, 1.0);
        float band = mix(0.12, sampleAudioSpectrum(iChannel0, samplePos), audioLive);
        float wave = mix(0.0, sampleAudioWaveform(iChannel0, fract(uv.x * 0.45 + 0.5 + fi * 0.13)), audioLive);
        float curve = lane
                    + sin(uv.x * (3.2 + fi * 0.22) + t * (1.4 + fi * 0.10) + fi * 1.15) * (0.055 + 0.035 * bass)
                    + sin(uv.x * 7.0 - t * 0.8 + fi) * (0.010 + 0.020 * band)
                    + wave * (0.006 + 0.014 * treble);
        float width = 0.010 + 0.010 * band + 0.004 * onset;
        float d = abs(uv.y - curve);
        float core = exp(-d * d / max(0.000025, width * width * 0.26));
        float halo = exp(-d * (30.0 - 8.0 * band));
        vec3 tint = ribbonPalette(fi * 0.13 + uv.x * 0.10 + iTime * 0.025);
        color += tint * core * (0.28 + 0.52 * band);
        color += tint * halo * (0.012 + 0.035 * bass);
    }

    float breathe = 0.90 + 0.10 * sin(iTime * 0.7) + 0.16 * iAudioBeatStrength * audioLive;
    color *= breathe;
    color *= 0.76 + 0.24 * exp(-1.2 * dot(uv, uv));
    color = 1.0 - exp(-color * 1.5);
    fragColor = vec4(color, 1.0);
}
