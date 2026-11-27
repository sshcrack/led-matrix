/* led-matrix-shader
{
  "family": "tunnel",
  "tags": ["music", "tunnel", "depth", "neon", "corridor", "equalizer", "audio-reactive", "geometric", "beat-driven", "showcase", "style:edm", "style:rock"],
  "intensity": 0.86,
  "motion": 0.92,
  "music_affinity": 1.0,
  "performance_cost": 0.38,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

// Flying down a long neon corridor whose side walls are equalizer displays.
// Every wall panel is a column of lit cells that rises with one band of the
// spectrum; the glossy floor reflects them, ceiling strip lights pulse with
// the beat and bass lights the floor grid. Kicks flash the frame arches
// as they pass; drops turn the whole corridor hot.

#define TAU 6.28318530718

float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

// Anti-aliased "near integer" line for a coordinate x, width w in x units.
float gridLine(float x, float w) {
    float d = abs(fract(x + 0.5) - 0.5);
    float fw = fwidth(x);
    return smoothstep(w + fw, w, d) * clamp(0.5 / max(fw * 6.0, 0.5), 0.0, 1.0);
}

// Height (0..1) of the equalizer column with index k.
float columnLevel(float k, float live) {
    float idle = 0.38 + 0.22 * sin(k * 1.7 + iTime * 1.3) + 0.12 * sin(k * 0.6 - iTime * 0.7);
    float u = 0.5 - 0.5 * cos(mod(k, 16.0) / 16.0 * TAU);   // seam-safe band sweep
    float band = sampleAudioSpectrum(iChannel0, 0.02 + 0.55 * u);
    return clamp(mix(idle, 0.08 + 1.05 * band, live), 0.0, 1.0);
}

vec3 eqColour(float row) {
    // Classic meter gradient pushed toward neon: cyan → violet → hot pink.
    return mix(mix(vec3(0.10, 0.85, 1.00), vec3(0.55, 0.30, 1.00), smoothstep(0.0, 0.55, row)),
               vec3(1.00, 0.20, 0.55), smoothstep(0.55, 1.0, row));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    float bass = mix(0.20 + 0.04 * sin(iTime * 1.1), iAudioBass, live);
    float beat = mix(0.25 + 0.15 * sin(iTime * 2.4), iAudioBeatStrength, live);
    float kick = live * iAudioKick;
    float drop = live * iAudioDrop;

    // Gentle camera sway; the vanishing point drifts a little.
    vec2 sway = vec2(0.05 * sin(iTime * 0.21), 0.03 * sin(iTime * 0.17));
    vec2 d = uv - sway;
    float roll = 0.05 * sin(iTime * 0.13);
    d = mat2(cos(roll), -sin(roll), sin(roll), cos(roll)) * d;

    const float W = 1.0;     // half width
    const float H = 0.70;    // half height
    const float F = 0.85;    // focal length
    float tx = W / max(abs(d.x), 1e-4);
    float ty = H / max(abs(d.y), 1e-4);
    float t = min(tx, ty) * F;          // depth of the hit
    // Constant flight speed: any audio term in the travel phase makes the
    // corridor lurch to a new depth when the bass changes. Music lives in the
    // walls, arches and lights instead.
    float travel = iTime * 1.5;
    float v = t + travel;               // position along the corridor
    float fog = exp(-t * 0.16);

    vec3 col = vec3(0.0);
    bool sideWall = tx < ty;
    // Frame arch every 4 units; the nearest one fades before it fills the view.
    float arch = gridLine(v / 4.0, 0.012) * smoothstep(0.5, 1.6, t);

    if (sideWall) {
        // Wall coordinate: y from floor (0) to ceiling (1).
        float y = (d.y * t / F + H) / (2.0 * H);
        float side = sign(d.x);
        float colIdx = floor(v * 1.0) + (side > 0.0 ? 100.0 : 0.0);
        float level = columnLevel(colIdx, live);
        float rows = 12.0;
        float row = floor(y * rows);
        float rowT = (row + 0.5) / rows;
        // Cell gap in both directions.
        float cellX = abs(fract(v) - 0.5);
        float cellY = abs(fract(y * rows) - 0.5);
        float fwX = fwidth(v), fwY = fwidth(y * rows);
        float cell = smoothstep(0.42 + fwX, 0.42, cellX) * smoothstep(0.36 + fwY, 0.36, cellY);
        // Fade cell detail into a soft bar where it gets too small to resolve.
        cell = mix(cell, 0.6, smoothstep(0.25, 0.6, fwX));
        float on = step(rowT, level) * step(0.06, y) * step(y, 0.94);
        float peak = step(abs(rowT - (floor(level * rows) + 0.5) / rows), 0.01);  // top cell
        vec3 ec = eqColour(rowT);
        col = vec3(0.012, 0.010, 0.025);                              // dark wall
        col += ec * cell * on * (0.80 + 0.35 * kick + 0.3 * drop);
        col += vec3(1.0, 0.85, 0.95) * cell * on * peak * 0.6;
        col += ec * cell * (1.0 - on) * 0.035;                         // unlit cells
        col += vec3(0.9, 0.3, 1.0) * arch * (0.5 + 0.3 * kick);
    } else {
        float x = d.x * t / F / W;           // -1..1 across
        bool floorHit = d.y < 0.0;
        if (floorHit) {
            // Glossy floor: grid plus blurred reflection of the EQ walls.
            float lane = gridLine(x * 3.0, 0.02) + gridLine(v, 0.02);
            col = vec3(0.010, 0.008, 0.020);
            col += vec3(0.35, 0.15, 0.80) * lane * (0.35 + 0.45 * bass);
            float edge = smoothstep(0.55, 1.0, abs(x));
            float k = floor(v) + (x > 0.0 ? 100.0 : 0.0);
            float lv = columnLevel(k, live);
            col += eqColour(0.2) * edge * lv * 0.30;
            col += vec3(0.9, 0.3, 1.0) * arch * (0.5 + 0.3 * kick);
        } else {
            // Ceiling: two strip lights running down the corridor.
            // Ceiling: a row of light panels down the middle.
            float panelX = smoothstep(0.20 + fwidth(x), 0.20, abs(x));
            float panelV = smoothstep(0.30 + fwidth(v), 0.30, abs(fract(v * 0.5) - 0.5));
            float pulse = 0.40 + 0.25 * beat + 0.15 * kick;
            col = vec3(0.012, 0.010, 0.026);
            col += vec3(0.75, 0.80, 1.0) * panelX * panelV * pulse * 0.25;
            col += vec3(0.9, 0.3, 1.0) * arch * (0.5 + 0.3 * kick);
        }
    }

    // Distance fog toward a warm vanishing glow; drops heat the far end.
    vec3 farGlow = mix(vec3(0.35, 0.12, 0.60), vec3(1.0, 0.30, 0.35), drop);
    col = col * fog + farGlow * (1.0 - fog) * 0.35;
    col += farGlow * exp(-dot(d, d) * 60.0) * (0.25 + 0.35 * beat);

    col = 1.0 - exp(-col * 1.6);
    fragColor = vec4(col, 1.0);
}
