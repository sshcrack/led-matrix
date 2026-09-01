/* led-matrix-shader
{
  "family": "orbital",
  "tags": ["music", "neon", "radial", "audio-reactive", "geometric"],
  "intensity": 0.78,
  "motion": 0.82,
  "music_affinity": 1.0,
  "performance_cost": 0.30,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

vec3 palette(float t) {
    vec3 a = vec3(0.50);
    vec3 b = vec3(0.50);
    vec3 c = vec3(1.00);
    vec3 d = vec3(0.00, 0.18, 0.42);
    return a + b * cos(6.2831853 * (c * t + d));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float radius = length(uv);
    float angle = atan(uv.y, uv.x);

    float hasMusic = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    float idleBass = 0.16 + 0.05 * sin(iTime * 1.4);
    float bass = mix(idleBass, iAudioBass, hasMusic);
    float beat = mix(0.18, iAudioBeatStrength, hasMusic);
    float spectrumPos = clamp(abs(angle) / 3.14159265, 0.0, 1.0);
    float spectrum = mix(0.12, sampleAudioSpectrum(iChannel0, spectrumPos), hasMusic);

    float spin = angle + iTime * (0.22 + 0.10 * iAudioMid);
    float wobble = sin(spin * 5.0 - iTime * 1.7) * (0.012 + 0.025 * bass);
    float orbit = 0.29 + wobble + 0.065 * bass;

    float ring = exp(-42.0 * abs(radius - orbit));
    float inner = exp(-55.0 * abs(radius - (0.14 + 0.025 * sin(spin * 3.0 + iTime))));
    float spectrumRim = exp(-34.0 * abs(radius - (0.39 + 0.09 * spectrum)));

    float spokes = pow(max(0.0, cos(spin * 8.0)), 14.0) * exp(-7.0 * radius);
    float core = exp(-18.0 * radius * radius) * (0.10 + 0.28 * beat);

    vec3 color = vec3(0.004, 0.006, 0.018);
    color += palette(0.08 * iTime + spectrumPos * 0.45) * ring * (0.72 + 0.58 * beat);
    color += palette(0.45 + spectrumPos * 0.35) * spectrumRim * (0.20 + 0.70 * spectrum);
    color += palette(0.72 + 0.05 * iTime) * inner * (0.25 + 0.45 * iAudioTreble);
    color += palette(0.15 + spectrumPos) * spokes * (0.04 + 0.11 * iAudioHihat);
    color += palette(0.9 + 0.1 * iTime) * core;

    color = 1.0 - exp(-color * 1.35);
    fragColor = vec4(color, 1.0);
}
