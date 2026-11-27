/* led-matrix-shader
{
  "family": "nixie",
  "tags": ["music", "nixie", "retro", "warm", "night", "audio-reactive", "beat-driven", "style:rock", "style:acoustic", "style:hiphop", "drop-reactive"],
  "intensity": 0.58,
  "motion": 0.52,
  "music_affinity": 1.0,
  "performance_cost": 0.30,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

// Four nixie tubes on a walnut plinth. Each tube's stacked cathodes sit dim
// behind a hex mesh; one numeral glows orange. Numerals are decoration only:
// every tube picks a random one on its own beat subdivision, so they never
// form a time, a count or any other readable value. Bass feeds the neon glow,
// kicks flare a tube, the snare blinks the indicator bulbs and a drop sends
// all tubes rolling through their cathodes.

#define TAU 6.28318530718

float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

float sdSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

// Distance to an ellipse outline (approximate, fine for thin strokes).
float sdEllipseLine(vec2 p, vec2 r) {
    float k = length(p / r);
    return abs(k - 1.0) * min(r.x, r.y);
}

// Ellipse outline kept only on the side where dot(p - c, n) > 0.
float arc(vec2 p, vec2 c, vec2 r, vec2 n) {
    vec2 q = p - c;
    return max(sdEllipseLine(q, r), -dot(q, n) * 0.6);
}

// Nixie numeral strokes. Glyph space: x in [-0.5, 0.5], y in [-0.8, 0.8].
float digitDist(int d, vec2 p) {
    float s = 1e3;
    if (d == 0) {
        s = sdEllipseLine(p, vec2(0.42, 0.74));
    } else if (d == 1) {
        s = min(sdSegment(p, vec2(0.05, -0.74), vec2(0.05, 0.74)), sdSegment(p, vec2(0.05, 0.74), vec2(-0.18, 0.52)));
    } else if (d == 2) {
        s = arc(p, vec2(0.0, 0.36), vec2(0.38, 0.36), vec2(0.15, 1.0));
        s = min(s, sdSegment(p, vec2(0.34, 0.24), vec2(-0.40, -0.74)));
        s = min(s, sdSegment(p, vec2(-0.40, -0.74), vec2(0.42, -0.74)));
    } else if (d == 3) {
        s = arc(p, vec2(0.0, 0.38), vec2(0.36, 0.36), vec2(1.0, 0.25));
        s = min(s, arc(p, vec2(0.0, -0.36), vec2(0.40, 0.38), vec2(1.0, -0.25)));
    } else if (d == 4) {
        s = sdSegment(p, vec2(0.20, -0.74), vec2(0.20, 0.74));
        s = min(s, sdSegment(p, vec2(0.20, 0.74), vec2(-0.42, -0.30)));
        s = min(s, sdSegment(p, vec2(-0.42, -0.30), vec2(0.44, -0.30)));
    } else if (d == 5) {
        s = sdSegment(p, vec2(0.38, 0.74), vec2(-0.30, 0.74));
        s = min(s, sdSegment(p, vec2(-0.30, 0.74), vec2(-0.36, 0.06)));
        s = min(s, arc(p, vec2(0.0, -0.30), vec2(0.40, 0.42), vec2(1.0, 0.0)));
        s = min(s, arc(p, vec2(0.0, -0.30), vec2(0.40, 0.42), vec2(0.0, 1.0)));
    } else if (d == 6) {
        s = sdEllipseLine(p - vec2(0.0, -0.36), vec2(0.40, 0.38));
        s = min(s, arc(p, vec2(0.30, -0.10), vec2(0.70, 0.84), vec2(-0.5, 1.0)));
    } else if (d == 7) {
        s = sdSegment(p, vec2(-0.42, 0.74), vec2(0.42, 0.74));
        s = min(s, sdSegment(p, vec2(0.42, 0.74), vec2(-0.12, -0.74)));
    } else if (d == 8) {
        s = sdEllipseLine(p - vec2(0.0, 0.40), vec2(0.32, 0.33));
        s = min(s, sdEllipseLine(p - vec2(0.0, -0.36), vec2(0.40, 0.38)));
    } else {
        s = sdEllipseLine(p - vec2(0.0, 0.36), vec2(0.40, 0.38));
        s = min(s, arc(p, vec2(-0.30, 0.10), vec2(0.70, 0.84), vec2(0.5, -1.0)));
    }
    return s;
}

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

// Hex mesh: 1 on the wires, 0 in the holes.
float hexMesh(vec2 p) {
    vec2 s = vec2(1.0, 1.7320508);
    vec2 a = mod(p, s) - s * 0.5;
    vec2 b = mod(p - s * 0.5, s) - s * 0.5;
    vec2 g = dot(a, a) < dot(b, b) ? a : b;
    g = abs(g);
    float edge = max(dot(g, s * 0.5), g.x);
    return smoothstep(0.36, 0.48, edge);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float px = 1.0 / iResolution.y;
    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));

    float bass = mix(0.30 + 0.06 * sin(iTime * 0.9), iAudioBass, live);
    float level = mix(0.35, iAudioLoudness, live);
    float kick = live * iAudioKick;
    float snare = live * iAudioSnare;
    float drop = live * max(iAudioDrop, iAudioSectionChange * 0.7);
    float bpm = mix(72.0, clamp(iAudioBpm, 60.0, 180.0), live);
    float beats = iTime * bpm / 60.0;

    vec3 orange = vec3(1.0, 0.36, 0.06);
    vec3 hot = vec3(1.0, 0.66, 0.30);

    // Dark warm room with out-of-focus lamp bokeh behind the tubes.
    vec3 col = mix(vec3(0.016, 0.010, 0.012), vec3(0.045, 0.024, 0.018), smoothstep(-0.5, 0.4, uv.y));
    for (int i = 0; i < 5; ++i) {
        float fi = float(i);
        vec2 c = vec2(-0.46 + fi * 0.23 + 0.02 * sin(iTime * 0.11 + fi), 0.38 - 0.05 * hash11(fi * 7.1));
        float b = smoothstep(0.075, 0.055, length(uv - c));
        col += mix(vec3(0.20, 0.08, 0.03), vec3(0.06, 0.05, 0.12), hash11(fi + 3.0)) * b * (0.30 + 0.20 * sin(iTime * 0.3 + fi * 2.0));
    }

    // Walnut plinth.
    float plinthTop = -0.30;
    if (uv.y < plinthTop) {
        float grain = 0.5 + 0.5 * sin(uv.x * 40.0 + 6.0 * sin(uv.y * 30.0 + uv.x * 3.0));
        vec3 wood = mix(vec3(0.075, 0.034, 0.016), vec3(0.13, 0.062, 0.028), grain);
        wood *= smoothstep(-0.56, plinthTop, uv.y) * 0.6 + 0.4;
        col = wood;
        // Front edge highlight.
        col += vec3(0.30, 0.16, 0.08) * smoothstep(1.5 * px, 0.0, abs(uv.y - plinthTop + px)) * 0.8;
    }

    const int Tubes = 4;
    float spacing = 0.235;
    vec3 glowSum = vec3(0.0);
    for (int i = 0; i < Tubes; ++i) {
        float fi = float(i);
        float x = (fi - 1.5) * spacing;
        vec2 p = uv - vec2(x, 0.02);

        // Each tube changes on its own subdivision with its own offset, so the
        // row never reads as a counter. A drop rolls through the cathodes.
        float rate = (i == 0) ? 0.25 : (i == 1) ? 0.5 : (i == 2) ? 0.333 : 1.0;
        float stepT = beats * rate + hash11(fi * 13.7) * 4.0;
        stepT = mix(stepT, iTime * 14.0 + fi * 3.3, smoothstep(0.35, 0.7, drop));
        float stepI = floor(stepT);
        float since = fract(stepT) / max(rate, 0.05) * 60.0 / bpm;  // seconds since change
        int cur = int(floor(hash11(stepI * 3.17 + fi * 41.0) * 10.0));
        int prev = int(floor(hash11((stepI - 1.0) * 3.17 + fi * 41.0) * 10.0));
        float swap = smoothstep(0.0, 0.07, since);

        // Band per tube drives brightness on top of the bass breath.
        float band = live * sampleAudioSpectrum(iChannel0, 0.03 + fi * 0.08);
        float flare = kick * step(abs(mod(floor(beats), 4.0) - fi), 0.5);
        float heat = 0.62 + 0.30 * bass + 0.25 * band + 0.55 * flare + 0.35 * drop;
        heat *= 0.96 + 0.04 * sin(iTime * 37.0 + fi * 5.0);  // neon shimmer

        // Glass envelope: round-topped cylinder.
        float glassD = sdRoundBox(p, vec2(0.092, 0.30), 0.085);
        // Exhaust tip on top of the dome.
        glassD = min(glassD, sdSegment(p, vec2(0.0, 0.29), vec2(0.0, 0.32)) - 0.012);
        float inside = smoothstep(px, -px, glassD);

        vec2 gq = p - vec2(0.0, 0.04);
        vec2 glyph = gq / 0.135;  // numeral ~0.2 tall, ~0.11 wide
        float dCur = digitDist(cur, glyph) * 0.135;
        float dPrev = digitDist(prev, glyph) * 0.135;

        // Stacked unlit cathodes: dim grey wire of every numeral behind.
        float cath = 1e3;
        for (int k = 0; k < 10; ++k) cath = min(cath, digitDist(k, glyph * (1.0 + 0.025 * float(k))) * 0.135);
        vec3 tube = vec3(0.020, 0.014, 0.014);
        tube += vec3(0.11, 0.09, 0.085) * smoothstep(1.2 * px, 0.2 * px, cath) * 0.55;
        // Lead-in wires and the mica spacer under the cathode stack.
        float leads = min(abs(abs(p.x) - 0.045), abs(p.x));
        float leadMask = step(p.y, -0.08) * step(-0.29, p.y);
        tube += vec3(0.16, 0.12, 0.10) * smoothstep(1.0 * px, 0.0, leads) * leadMask * (0.5 + 0.3 * heat);
        tube += vec3(0.10, 0.08, 0.07) * smoothstep(1.2 * px, 0.0, abs(p.y + 0.15)) * step(abs(p.x), 0.07);

        // Lit numeral: hot core, orange neon sheath, halo inside the glass.
        float coreW = 0.85 * px;
        float lit = mix(smoothstep(coreW + px, coreW, dPrev), 0.0, swap) + smoothstep(coreW + px, coreW, dCur) * swap;
        float glow = mix(exp(-dPrev / (3.2 * px)), 0.0, swap) + exp(-dCur / (3.2 * px)) * swap;
        float wide = mix(exp(-dPrev / (11.0 * px)), 0.0, swap) + exp(-dCur / (11.0 * px)) * swap;
        tube = mix(tube, hot, clamp(lit * heat, 0.0, 1.0));
        tube += orange * glow * heat * 0.75;
        tube += orange * wide * heat * 0.22;

        // Anode mesh in front of the numerals catches the glow.
        float mesh = hexMesh(gq / (3.0 * px)) * smoothstep(0.012, 0.0, sdRoundBox(gq, vec2(0.075, 0.13), 0.02));
        tube = mix(tube, tube * 0.55 + orange * 0.10 * heat * wide, mesh * 0.55);

        // Glass: rim, specular streak and the exhaust tip on the dome.
        float rim = smoothstep(1.6 * px, 0.0, abs(glassD));
        tube += vec3(0.30, 0.22, 0.18) * rim * 0.35;
        tube += vec3(0.55, 0.50, 0.45) * smoothstep(1.6 * px, 0.0, abs(p.x + 0.060)) * smoothstep(0.24, 0.05, abs(p.y - 0.06)) * 0.30;
        tube += orange * rim * heat * 0.18;

        // Ceramic-and-steel base where the pins enter the plinth.
        float base = step(abs(p.x), 0.095) * step(plinthTop - 0.02, uv.y) * step(uv.y, plinthTop + 0.045);
        vec3 baseCol = vec3(0.09, 0.08, 0.075) + vec3(0.22, 0.18, 0.15) * smoothstep(0.03, 0.0, abs(p.x + 0.05)) * 0.6;

        col = mix(col, tube, inside);
        col = mix(col, baseCol, base);

        // Light thrown around the tube and onto the plinth.
        float outside = 1.0 - inside;
        glowSum += orange * exp(-max(glassD, 0.0) / 0.05) * outside * heat * 0.10;
        float pool = exp(-abs(uv.x - x) * 14.0) * smoothstep(plinthTop, plinthTop - 0.14, uv.y) * step(uv.y, plinthTop);
        glowSum += orange * pool * heat * 0.22;
    }
    col += glowSum * (0.8 + 0.4 * level);

    // Two indicator bulbs on the plinth blink with the snare.
    for (int j = 0; j < 2; ++j) {
        vec2 c = vec2(j == 0 ? -0.43 : 0.43, -0.41);
        float b = smoothstep(0.020, 0.012, length(uv - c));
        float on = mix(0.25 + 0.15 * sin(iTime * 1.3 + float(j) * 3.0), 0.25 + snare, live);
        col += vec3(1.0, 0.30, 0.08) * b * on;
        col += vec3(1.0, 0.30, 0.08) * exp(-length(uv - c) * 30.0) * on * 0.25;
    }

    col = 1.0 - exp(-col * 1.5);
    fragColor = vec4(col, 1.0);
}
