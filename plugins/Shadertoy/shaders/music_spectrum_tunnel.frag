/* led-matrix-shader
{
  "family": "tunnel",
  "tags": ["music", "tunnel", "depth", "neon", "audio-reactive", "geometric", "beat-driven"],
  "intensity": 0.86,
  "motion": 0.94,
  "music_affinity": 1.0,
  "performance_cost": 0.38,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

#define TAU 6.28318530718

vec3 tunnelPalette(float t) {
    return 0.48 + 0.52 * cos(TAU * (vec3(0.02, 0.22, 0.55) + t + vec3(0.0, 0.16, 0.31)));
}

float ringLine(float d, float width) {
    return exp(-d * d / max(0.0002, width));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float audioLive = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    float bass = mix(0.18 + 0.05 * sin(iTime * 1.1), iAudioBass, audioLive);
    float mid = mix(0.16, iAudioMid, audioLive);
    float beat = mix(0.20, iAudioBeatStrength, audioLive);
    float phase = mix(fract(iTime * 0.42), iAudioBeatPhase, step(0.35, iAudioBeatConfidence) * audioLive);

    float travel = iTime * (0.68 + bass * 0.46);
    vec3 col = vec3(0.002, 0.004, 0.012);
    float glow = 0.0;

    for (int i = 0; i < 12; ++i) {
        float fi = float(i);
        float z = fract(fi / 12.0 - travel * 0.16);
        float depth = 0.18 + z * 1.08;
        float perspective = 0.085 / depth;
        float twist = travel * 0.35 + fi * 0.58 + mid * 0.8;
        vec2 center = vec2(sin(twist), cos(twist * 0.83)) * (0.045 + 0.035 * bass) * (1.0 - z);
        vec2 p = uv - center;
        float angle = atan(p.y, p.x);
        float spectrumPos = fract(angle / TAU + 0.5);
        float spectrum = mix(0.12, sampleAudioSpectrum(iChannel0, spectrumPos), audioLive);
        float target = perspective * (1.0 + 0.30 * spectrum + 0.10 * beat * (1.0 - z));
        float radial = abs(length(p) - target);
        float line = ringLine(radial, 0.000010 + 0.000028 * z);
        float ribs = pow(max(0.0, cos(angle * 8.0 + fi * 0.72 - travel)), 18.0) * line;
        float fade = smoothstep(1.0, 0.08, z) * smoothstep(0.0, 0.12, z);
        vec3 tint = tunnelPalette(fi * 0.055 + spectrumPos * 0.18 + iTime * 0.025);
        col += tint * line * fade * (0.18 + 0.58 * spectrum);
        col += tint * ribs * fade * (0.04 + 0.28 * iAudioHihat);
        glow += line * fade;
    }

    float centerPulse = exp(-14.0 * dot(uv, uv)) * (0.02 + 0.18 * beat * (0.55 + 0.45 * cos(TAU * phase)));
    col += tunnelPalette(0.7 + iTime * 0.04) * centerPulse;
    col += tunnelPalette(0.1 + bass * 0.2) * glow * glow * 0.012;
    col = 1.0 - exp(-col * 1.55);
    fragColor = vec4(col, 1.0);
}
