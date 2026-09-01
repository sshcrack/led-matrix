/* led-matrix-shader
{
  "family": "tunnel",
  "tags": ["music", "tunnel", "depth", "neon", "audio-reactive", "geometric", "beat-driven", "showcase"],
  "intensity": 0.88,
  "motion": 0.94,
  "music_affinity": 1.0,
  "performance_cost": 0.40,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

#define TAU 6.28318530718
#define PI 3.14159265359

vec3 tunnelPalette(float t) {
    vec3 a = vec3(0.48, 0.50, 0.55);
    vec3 b = vec3(0.50, 0.48, 0.44);
    vec3 c = vec3(1.0);
    vec3 d = vec3(0.02, 0.18, 0.46);
    return a + b * cos(TAU * (c * t + d));
}

float circularSpectrum(float angle, float live) {
    // Do not map atan() directly to 0..1: +/-PI are the same direction but
    // would otherwise sample opposite ends of the FFT and visibly split rings.
    float u = 0.5 - 0.5 * cos(angle);
    float primary = sampleAudioSpectrum(iChannel0, u);
    float secondary = sampleAudioSpectrum(iChannel0, 1.0 - u);
    return mix(0.14, mix(primary, secondary, 0.28), live);
}

float lineGlow(float distanceToLine, float width) {
    float x = distanceToLine / max(width, 0.0001);
    return exp(-x * x * 1.7);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    float bass = mix(0.20 + 0.04 * sin(iTime * 1.1), iAudioBass, live);
    float mid = mix(0.16 + 0.03 * sin(iTime * 0.8 + 1.0), iAudioMid, live);
    float treble = mix(0.10, iAudioTreble, live);
    float beat = mix(0.16 + 0.04 * sin(iTime * 1.7), iAudioBeatStrength, live);
    float kick = mix(0.12, iAudioKick, live);

    float travel = iTime * (0.46 + 0.30 * bass);
    vec2 vanish = vec2(0.055 * sin(iTime * 0.23), 0.030 * cos(iTime * 0.19));
    vec3 col = vec3(0.002, 0.004, 0.012);

    // Faint central atmosphere makes the tunnel read as volume instead of a
    // stack of disconnected rings, while keeping the physical matrix mostly dark.
    float centerFog = exp(-5.4 * dot(uv - vanish, uv - vanish));
    col += tunnelPalette(0.63 + iTime * 0.018) * centerFog * (0.018 + 0.032 * bass);

    for (int i = 0; i < 14; ++i) {
        float fi = float(i);
        // z=0 is the camera end and z=1 is the vanishing point. Rings fade to
        // zero at BOTH endpoints, so fract() wrapping is visually continuous.
        float z = fract(fi / 14.0 - travel * 0.145);
        float endpointFade = smoothstep(0.025, 0.13, z)
                           * (1.0 - smoothstep(0.84, 0.985, z));

        float perspective = pow(z, 0.72);
        float radius = mix(1.12, 0.065, perspective);
        float twist = fi * 0.49 + travel * 0.52 + mid * 0.72;
        vec2 center = vanish
                    + vec2(sin(twist * 0.83), cos(twist * 0.71))
                    * (0.052 + 0.035 * bass) * (1.0 - perspective);
        vec2 p = uv - center;
        float angle = atan(p.y, p.x);
        float spectrum = circularSpectrum(angle + twist * 0.09, live);

        // Spectrum deforms the entire closed ring continuously rather than
        // changing the two sides of the atan seam independently.
        float ripple = 1.0
                     + 0.10 * spectrum
                     + 0.022 * sin(angle * 5.0 + twist)
                     + 0.014 * sin(angle * 9.0 - travel * 0.8);
        float target = radius * ripple;
        float d = abs(length(p) - target);
        float width = mix(0.010, 0.0035, perspective) + 0.0025 * kick * (1.0 - perspective);
        float ring = lineGlow(d, width);
        float halo = exp(-d / max(width * 4.8, 0.001));

        float ribs = pow(max(0.0, 0.5 + 0.5 * cos(angle * 8.0 + fi * 0.63 - travel * 1.1)), 10.0);
        float ribAccent = ring * ribs * (0.12 + 0.30 * treble);
        vec3 tint = tunnelPalette(fi * 0.047 + 0.09 * spectrum + iTime * 0.018);

        float depthLight = mix(0.88, 0.28, perspective);
        col += tint * ring * endpointFade * depthLight * (0.40 + 0.62 * spectrum);
        col += tint * halo * endpointFade * (0.018 + 0.040 * bass);
        col += tint * ribAccent * endpointFade;
    }

    // Stable radial guide streaks reinforce depth without introducing another
    // wrap seam. Their brightness is modest even on a kick.
    vec2 q = uv - vanish;
    float a = atan(q.y, q.x);
    float r = length(q);
    float spokes = pow(max(0.0, 0.5 + 0.5 * cos(a * 12.0 - travel * 0.7)), 22.0);
    spokes *= smoothstep(0.08, 0.22, r) * (1.0 - smoothstep(0.72, 1.05, r));
    col += tunnelPalette(0.17 + a / TAU) * spokes * (0.010 + 0.020 * treble);

    float pulse = exp(-18.0 * dot(q, q)) * (0.028 + 0.12 * beat + 0.10 * kick);
    col += tunnelPalette(0.80 + iTime * 0.025) * pulse;

    col = 1.0 - exp(-col * 1.48);
    fragColor = vec4(col, 1.0);
}
