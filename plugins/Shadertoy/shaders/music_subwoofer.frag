/* led-matrix-shader
{
  "family": "speaker",
  "tags": ["music", "speaker", "bass", "neon", "night", "audio-reactive", "beat-driven", "style:hiphop", "style:party", "style:edm", "drop-reactive"],
  "intensity": 0.66,
  "motion": 0.55,
  "music_affinity": 1.0,
  "performance_cost": 0.25,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

// A big subwoofer in a dark room lit by neon. The cone pumps with the kick
// and bass, the surround ripples, dust jumps off the cone on drops and the
// neon rim breathes with the beat.

#define TAU 6.28318530718

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

float spectrum(float x) { return texture(iChannel0, vec2(x, 0.25)).r; }

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));

    // Idle: a slow heartbeat so the room never freezes.
    float idlePulse = pow(max(0.0, sin(iTime * 2.2)), 8.0) * 0.35;
    float kick = mix(idlePulse, max(iAudioKick, iAudioBass * 0.7), live);
    float bass = mix(0.15, iAudioBass, live);
    float beat = mix(0.1, iAudioBeatStrength, live);
    float drop = live * iAudioDrop;
    // Curated neon pairs (no muddy in-between hues), slowly alternating.
    float pick = smoothstep(0.35, 0.65, 0.5 + 0.5 * sin(iTime * 0.045));
    vec3 neonA = mix(vec3(0.72, 0.18, 1.00), vec3(1.00, 0.16, 0.55), pick);
    vec3 neonB = mix(vec3(0.15, 0.78, 1.00), vec3(0.35, 0.40, 1.00), pick);

    // Room: dark wall with soft neon wash from both sides.
    vec3 col = vec3(0.02, 0.015, 0.035);
    col += neonA * 0.22 * exp(-abs(uv.x + 0.85) * 2.5) * (0.6 + 0.4 * beat);
    col += neonB * 0.22 * exp(-abs(uv.x - 0.85) * 2.5) * (0.6 + 0.4 * beat);

    // Cabinet.
    vec2 c = uv - vec2(0.0, -0.02);
    vec2 box = abs(c) - vec2(0.43, 0.46);
    float cab = max(box.x, box.y);
    float cabMask = smoothstep(0.004, -0.004, cab);
    col = mix(col, vec3(0.018, 0.016, 0.024), cabMask);
    // Rim light on the cabinet edge.
    col += neonA * smoothstep(0.012, 0.0, abs(cab)) * (0.45 + 0.55 * beat) * step(c.x, 0.0);
    col += neonB * smoothstep(0.012, 0.0, abs(cab)) * (0.45 + 0.55 * beat) * step(0.0, c.x);

    // Driver: radial distance, perspective-free.
    float r = length(c);
    float R = 0.37;
    // Cone excursion: pushes the shading outward and enlarges the dust cap.
    float exc = 0.03 * kick + 0.012 * bass;
    if (r < R + 0.02) {
        // Rubber surround (half-roll torus) rippling with the kick.
        float s = (r - (R - 0.05)) / 0.07;
        float surround = smoothstep(0.0, 0.15, s) * smoothstep(1.0, 0.85, s);
        float ripple = 0.5 + 0.5 * sin(s * 3.14159 + iTime * 6.0 * kick);
        vec3 rubber = vec3(0.05, 0.05, 0.06) + neonA * 0.18 * ripple * (0.3 + kick);

        // Cone: concentric shading lit from the upper left; bright when pushed out.
        vec2 n = normalize(c + 1e-4);
        float lightDir = dot(n, normalize(vec2(-0.6, 0.8)));
        float rings = 0.5 + 0.5 * sin(r * 70.0 - kick * 6.0);
        float cone = 0.10 + 0.08 * lightDir * (r / R) + 0.03 * rings;
        vec3 coneCol = vec3(cone) * vec3(0.9, 0.9, 1.05);
        // Spectrum shimmer around the cone: angle mirrored, so no seam.
        float a = 0.5 - 0.5 * cos(atan(c.y, c.x));
        float spec = live * spectrum(0.03 + a * 0.35);
        coneCol += mix(neonA, neonB, a) * spec * 0.45 * smoothstep(R - 0.05, R * 0.4, r);
        coneCol += neonB * kick * 0.30 * smoothstep(R, 0.0, r);

        // Dust cap.
        float capR = 0.11 + exc;
        float cap = smoothstep(capR + 0.006, capR - 0.006, r);
        vec3 capCol = vec3(0.10, 0.10, 0.12) + vec3(0.25) * pow(max(0.0, dot(n, vec2(-0.6, 0.6))) * (1.0 - r / capR), 2.0);
        capCol += neonA * 0.25 * kick;

        vec3 driver = mix(coneCol, rubber, surround * step(R - 0.05, r));
        driver = mix(driver, capCol, cap);
        float driverMask = smoothstep(R + 0.02, R + 0.01, r);
        col = mix(col, driver, driverMask);
        // Steel frame ring.
        col += vec3(0.25, 0.25, 0.3) * smoothstep(0.008, 0.0, abs(r - (R + 0.015))) * (0.5 + 0.5 * beat);
    }

    // Mounting bolts.
    for (int i = 0; i < 8; ++i) {
        float ang = float(i) * TAU / 8.0 + 0.39;
        vec2 bp = vec2(cos(ang), sin(ang)) * (R + 0.045);
        col += vec3(0.4) * smoothstep(0.012, 0.004, length(c - bp)) * (0.4 + 0.6 * beat);
    }

    // Dust motes thrown off the cone on kicks and drops.
    float motes = 0.0;
    for (int i = 0; i < 14; ++i) {
        float fi = float(i);
        float t = fract(iTime * 0.35 + hash21(vec2(fi, 3.1)));
        float ang = hash21(vec2(fi, 7.7)) * TAU;
        float dist = 0.12 + t * (0.25 + 0.25 * drop + 0.12 * kick);
        vec2 p = vec2(cos(ang), sin(ang)) * dist;
        motes += smoothstep(0.012, 0.0, length(c - p)) * (1.0 - t) * (0.25 + 0.75 * max(kick, drop));
    }
    col += neonB * motes * 0.8;

    // Floor reflection hint under the cabinet.
    col += neonA * 0.05 * smoothstep(-0.48, -0.5, uv.y) * (0.5 + kick);

    fragColor = vec4(pow(col, vec3(0.92)), 1.0);
}
