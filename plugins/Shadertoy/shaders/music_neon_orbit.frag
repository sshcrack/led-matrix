/* led-matrix-shader
{
  "family": "orbital",
  "tags": ["music", "neon", "radial", "audio-reactive", "geometric", "showcase"],
  "intensity": 0.80,
  "motion": 0.82,
  "music_affinity": 1.0,
  "performance_cost": 0.31,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

#define TAU 6.28318530718
#define PI 3.14159265359

vec3 orbitPalette(float t) {
    vec3 a = vec3(0.50);
    vec3 b = vec3(0.50);
    vec3 c = vec3(1.00);
    vec3 d = vec3(0.00, 0.18, 0.42);
    return a + b * cos(TAU * (c * t + d));
}

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float r = length(uv);
    float angle = atan(uv.y, uv.x);

    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    float bass = mix(0.18 + 0.045 * sin(iTime * 1.15), iAudioBass, live);
    float mid = mix(0.14 + 0.025 * sin(iTime * 0.83), iAudioMid, live);
    float treble = mix(0.10, iAudioTreble, live);
    float beat = mix(0.14, iAudioBeatStrength, live);
    float kick = mix(0.10, iAudioKick, live);
    float spectrumPos = 0.5 - 0.5 * cos(angle);
    float spectrum = mix(0.13, sampleAudioSpectrum(iChannel0, spectrumPos), live);

    float spin = angle + iTime * (0.20 + 0.11 * mid);
    float wobble = sin(spin * 5.0 - iTime * 1.55) * (0.014 + 0.024 * bass)
                 + sin(spin * 9.0 + iTime * 0.61) * 0.006;
    float orbitRadius = 0.335 + wobble + 0.060 * bass;
    float outerRadius = 0.445 + 0.060 * spectrum + 0.018 * beat;
    float innerRadius = 0.165 + 0.024 * sin(spin * 3.0 + iTime * 0.72) + 0.018 * bass;

    float ring = exp(-46.0 * abs(r - orbitRadius));
    float ringHalo = exp(-15.0 * abs(r - orbitRadius));
    float outer = exp(-37.0 * abs(r - outerRadius));
    float inner = exp(-54.0 * abs(r - innerRadius));

    float arcMask = pow(0.5 + 0.5 * cos(spin * 3.0 - iTime * 0.36), 5.0);
    float orbitArcs = exp(-30.0 * abs(r - (0.515 + 0.014 * sin(spin * 2.0 + iTime)))) * arcMask;
    float spokes = pow(max(0.0, 0.5 + 0.5 * cos(spin * 8.0)), 13.0) * exp(-5.5 * r);
    float core = exp(-16.0 * r * r) * (0.10 + 0.24 * beat + 0.16 * kick);

    vec3 color = vec3(0.0025, 0.004, 0.014);
    color += orbitPalette(0.08 * iTime + spectrumPos * 0.44) * ring * (0.62 + 0.48 * beat);
    color += orbitPalette(0.08 * iTime + spectrumPos * 0.44) * ringHalo * (0.020 + 0.035 * bass);
    color += orbitPalette(0.45 + spectrumPos * 0.35) * outer * (0.16 + 0.60 * spectrum);
    color += orbitPalette(0.72 + 0.05 * iTime) * inner * (0.24 + 0.40 * treble);
    color += orbitPalette(0.16 + spectrumPos) * spokes * (0.025 + 0.090 * treble);
    color += orbitPalette(0.92 + 0.08 * iTime) * orbitArcs * (0.09 + 0.10 * mid);
    color += orbitPalette(0.90 + 0.08 * iTime) * core;

    // Dim orbital dust makes the scene fill the matrix while preserving the
    // crisp central identity that made the original shader work well.
    vec2 cell = floor((uv + vec2(1.1, 0.55)) * vec2(22.0, 15.0));
    float seed = hash21(cell);
    vec2 local = fract((uv + vec2(1.1, 0.55)) * vec2(22.0, 15.0)) - 0.5;
    float dust = step(0.972, seed) * exp(-42.0 * dot(local, local));
    dust *= smoothstep(0.24, 0.85, r) * (0.45 + 0.55 * sin(iTime * 0.5 + seed * TAU) * sin(iTime * 0.5 + seed * TAU));
    color += orbitPalette(seed + 0.17) * dust * (0.025 + 0.055 * treble);

    color = 1.0 - exp(-color * 1.38);
    fragColor = vec4(color, 1.0);
}
