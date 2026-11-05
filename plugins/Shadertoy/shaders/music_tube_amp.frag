/* led-matrix-shader
{
  "family": "tube-amp",
  "tags": ["music", "amplifier", "warm", "night", "audio-reactive", "beat-driven", "calm", "style:rock", "style:acoustic", "drop-reactive"],
  "intensity": 0.52,
  "motion": 0.40,
  "music_affinity": 1.0,
  "performance_cost": 0.22,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

// The back of a vintage guitar amp: a row of glowing vacuum tubes. Each tube
// is one frequency band (its filament and blue-violet getter glow follow the
// band), the warm halo breathes with the loudness, a snare ticks the plate
// and drops flare all tubes at once.

#define TAU 6.28318530718

float spectrum(float x) { return texture(iChannel0, vec2(clamp(x, 0.0, 1.0), 0.25)).r; }

float sdCapsule(vec2 p, float halfH, float r) {
    p.y -= clamp(p.y, -halfH, halfH);
    return length(p) - r;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    float level = mix(0.25 + 0.05 * sin(iTime * 0.7), iAudioLoudness, live);
    float snare = live * iAudioSnare;
    float drop = live * iAudioDrop;
    float beat = live * iAudioBeatStrength;

    // Chassis: brushed dark metal back panel, warm light spill from the tubes.
    vec3 col = vec3(0.035, 0.032, 0.030) * (0.85 + 0.15 * sin(uv.y * 160.0 + sin(uv.x * 9.0)));
    // Chassis deck the tubes stand on.
    float deckY = -0.34;
    col = mix(col, vec3(0.08, 0.075, 0.07), smoothstep(deckY + 0.004, deckY - 0.004, uv.y));
    col += vec3(0.25, 0.22, 0.2) * smoothstep(0.006, 0.0, abs(uv.y - deckY)) * 0.6;

    const int Tubes = 5;
    vec3 glowSum = vec3(0.0);
    for (int i = 0; i < Tubes; ++i) {
        float fi = float(i);
        float x = (fi - 2.0) * 0.19;
        float tall = (i == 0 || i == Tubes - 1) ? 0.29 : 0.23;  // power tubes on the ends
        float width = (i == 0 || i == Tubes - 1) ? 0.080 : 0.064;
        vec2 p = uv - vec2(x, deckY + tall + 0.01);

        // Band per tube: idle flickers gently, live follows the spectrum.
        float band = live * spectrum(0.02 + fi * 0.09);
        float idle = 0.55 + 0.10 * sin(iTime * (1.3 + fi * 0.37) + fi * 2.0);
        float heat = clamp(mix(idle, 0.25 + band * 1.1, live) + drop * 0.6, 0.0, 1.4);

        float d = sdCapsule(p, tall - width, width);
        float glass = smoothstep(0.004, -0.004, d);
        // Glass: dark with a vertical specular strip.
        vec3 tube = vec3(0.04, 0.035, 0.04);
        tube += vec3(0.35, 0.33, 0.30) * smoothstep(0.012, 0.0, abs(p.x + width * 0.45)) * 0.35;
        // Internal plate structure (dark grey box) with the filament glow behind.
        float plate = step(abs(p.x), width * 0.55) * step(abs(p.y + 0.01), tall * 0.55);
        float fil = exp(-abs(p.x) * 60.0) * smoothstep(tall * 0.7, 0.0, abs(p.y + 0.01));
        vec3 orange = vec3(1.0, 0.42, 0.10);
        tube += orange * fil * heat * 1.6;
        tube = mix(tube, vec3(0.10, 0.09, 0.09) + orange * 0.15 * heat, plate * 0.65);
        // Plate edge glints on the snare.
        tube += vec3(1.0, 0.7, 0.45) * plate * smoothstep(0.006, 0.0, abs(abs(p.x) - width * 0.55)) * snare * 0.8;
        // Getter flash: violet-blue glow near the top (classic tube look).
        float getter = exp(-length(p - vec2(0.0, tall - width * 1.2)) * 28.0);
        tube += vec3(0.45, 0.35, 1.0) * getter * (0.25 + 0.75 * heat * heat);
        // Socket.
        float socket = step(abs(p.x), width * 1.1) * step(abs(uv.y - deckY - 0.012), 0.012);
        col = mix(col, vec3(0.12, 0.10, 0.08), socket);

        col = mix(col, tube, glass);
        // Halo onto the chassis behind the tube.
        float halo = exp(-max(d, 0.0) * 16.0) * (1.0 - glass);
        glowSum += orange * halo * heat * 0.22;
    }
    col += glowSum * (0.7 + 0.6 * level);

    // Warm light pooling on the deck.
    col += vec3(1.0, 0.45, 0.15) * 0.08 * (0.6 + level) * smoothstep(deckY, deckY - 0.25, uv.y) * smoothstep(-0.9, 0.0, -abs(uv.x));

    // Power jewel lamp: pulses gently with the beat.
    col += vec3(1.0, 0.12, 0.08) * smoothstep(0.03, 0.0, length(uv - vec2(0.40, -0.43))) * (0.45 + 0.55 * beat);

    fragColor = vec4(col, 1.0);
}
