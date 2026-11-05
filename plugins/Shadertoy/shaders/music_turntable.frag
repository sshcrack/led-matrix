/* led-matrix-shader
{
  "family": "turntable",
  "tags": ["music", "turntable", "vinyl", "night", "audio-reactive", "beat-driven", "calm", "style:hiphop", "style:party", "style:chill"],
  "intensity": 0.48,
  "motion": 0.45,
  "music_affinity": 1.0,
  "performance_cost": 0.22,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

// A vinyl record spinning on a turntable, seen at an angle under a warm desk
// lamp. The groove sheen is the spectrum (low bins near the label, highs at
// the rim), the platter strobe dots step with the beat and the needle
// glints on kicks.

#define TAU 6.28318530718

float spectrum(float x) { return texture(iChannel0, vec2(clamp(x, 0.0, 1.0), 0.25)).r; }

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    float kick = live * max(iAudioKick, iAudioOnset * 0.5);
    float beat = mix(0.0, iAudioBeatStrength, live);
    float level = mix(0.15, iAudioLoudness, live);
    float drop = live * iAudioDrop;

    // Desk: dark wood lit by a warm lamp from the upper right.
    vec3 lamp = vec3(1.0, 0.72, 0.42);
    float grain = 0.5 + 0.5 * sin(uv.y * 90.0 + sin(uv.x * 7.0) * 2.0);
    vec3 col = vec3(0.045, 0.028, 0.018) * (0.8 + 0.2 * grain);
    col += lamp * 0.10 * exp(-length(uv - vec2(0.55, 0.55)) * 2.2);

    // Plinth.
    vec2 pb = abs(uv - vec2(0.0, -0.02)) - vec2(0.47, 0.40);
    float plinth = max(pb.x, pb.y);
    col = mix(col, vec3(0.06, 0.06, 0.07), smoothstep(0.006, -0.006, plinth));
    col += vec3(0.30, 0.28, 0.25) * smoothstep(0.01, 0.0, abs(plinth)) * 0.4;

    // Platter seen at an angle: squash y.
    vec2 c = uv - vec2(-0.06, -0.02);
    vec2 e = vec2(c.x, c.y / 0.82);
    float r = length(e);
    float ang = atan(e.y, e.x);
    float rpm = mix(0.33, 0.33 * (1.0 + 0.1 * drop), live);   // 33 1/3
    float spin = iTime * TAU * rpm * 0.25;                   // slowed for the matrix

    // Platter rim with strobe dots that step with the beat phase.
    float rimR = 0.36;
    if (r < rimR + 0.03) {
        float rim = smoothstep(rimR + 0.03, rimR + 0.02, r) * smoothstep(rimR, rimR + 0.006, r);
        float dots = step(0.5, fract((ang + spin * 0.0) / TAU * 48.0 + floor(iAudioBeatPhase * 4.0 + iTime * (1.0 - live)) * 0.25));
        vec3 rimCol = vec3(0.30, 0.30, 0.33) * (0.6 + 0.4 * dots);
        rimCol += vec3(1.0, 0.25, 0.20) * dots * 0.25 * (0.4 + 0.6 * beat);
        col = mix(col, rimCol, rim);
    }

    // Record.
    float recR = 0.34;
    if (r < recR) {
        float rr = r / recR;
        // Grooves: fine rings, broken into tracks.
        float groove = 0.5 + 0.5 * sin(r * 520.0);
        float track = smoothstep(0.02, 0.0, abs(fract(rr * 5.0) - 0.5) - 0.47);
        vec3 vinyl = vec3(0.035, 0.035, 0.042) + vec3(0.03) * groove;
        // Anisotropic lamp sheen: two bright wedges that stay fixed while the
        // record spins underneath (like a real record), widened by loudness.
        float sheenAng = cos(2.0 * (ang - 0.75));
        float sheen = pow(max(0.0, sheenAng), 7.0 - 4.0 * level);
        // Spectrum along the radius: bass near the label, highs at the edge.
        float spec = live * spectrum(0.02 + (rr - 0.32) / 0.68 * 0.5);
        vec3 sheenCol = mix(lamp, vec3(0.75, 0.55, 1.0), rr) * (0.30 + 0.90 * spec);
        vinyl += sheenCol * sheen * (0.7 + 0.3 * groove) * smoothstep(0.30, 0.36, rr);
        vinyl *= 1.0 - 0.6 * track;
        col = mix(col, vinyl, smoothstep(recR, recR - 0.006, r));

        // Label: spins (a stripe makes the rotation visible), glows on kicks.
        float labelR = 0.10;
        if (r < labelR) {
            float stripe = smoothstep(0.35, 0.15, abs(sin(0.5 * (ang - spin))));
            vec3 label = mix(vec3(0.85, 0.22, 0.18), vec3(0.95, 0.75, 0.30), stripe * 0.8);
            label *= 0.55 + 0.45 * smoothstep(labelR, 0.0, r) + 0.3 * kick;
            label = mix(label, vec3(0.8), smoothstep(0.012, 0.006, r));  // spindle
            col = mix(col, label, smoothstep(labelR, labelR - 0.006, r));
        }
    }

    // Tonearm from the upper-right pivot to the stylus on the record.
    vec2 pivot = vec2(0.33, 0.27);
    vec2 stylus = vec2(0.10, -0.12);
    vec2 pa = uv - pivot, ba = stylus - pivot;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    float arm = length(pa - ba * h);
    col = mix(col, vec3(0.62, 0.62, 0.66), smoothstep(0.011, 0.005, arm));
    col += vec3(0.5) * smoothstep(0.006, 0.0, arm) * 0.3;
    // Counterweight and pivot.
    col = mix(col, vec3(0.20, 0.20, 0.22), smoothstep(0.045, 0.038, length(uv - pivot)));
    col = mix(col, vec3(0.35), smoothstep(0.02, 0.014, length(uv - pivot)));
    // Headshell + glint on kicks.
    col = mix(col, vec3(0.15), smoothstep(0.03, 0.022, length(uv - stylus)));
    col += vec3(1.0, 0.9, 0.8) * smoothstep(0.035, 0.0, length(uv - stylus)) * (0.15 + 0.9 * kick);

    // Power LED.
    col += vec3(1.0, 0.15, 0.1) * smoothstep(0.016, 0.0, length(uv - vec2(-0.40, -0.35))) * (0.5 + 0.5 * beat);

    fragColor = vec4(col, 1.0);
}
