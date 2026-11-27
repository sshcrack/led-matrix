/* led-matrix-shader
{
  "family": "speaker",
  "tags": ["music", "speaker", "bass", "neon", "night", "audio-reactive", "beat-driven", "style:hiphop", "style:party", "style:edm", "drop-reactive"],
  "intensity": 0.72,
  "motion": 0.62,
  "music_affinity": 1.0,
  "performance_cost": 0.25,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

// A big car-audio subwoofer seen head-on, lit by a hard white key light from
// the upper left and a neon LED ring. Every kick throws the cone forward: the
// dust cap swells, the cone's highlight slides outward, the rubber surround
// rolls over, the cabinet shakes a pixel and the air ripples. Bass holds the
// cone out; drops shake dust off the cone and flood the LED ring.

#define TAU 6.28318530718

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float px = 1.0 / iResolution.y;
    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));

    // Idle: a slow double heartbeat so the cone still moves without music.
    float hb = fract(iTime * 0.55);
    // Each beat eases in over a few frames instead of snapping on.
    float idleKick = smoothstep(0.0, 0.05, hb) * exp(-hb * 9.0)
                   + 0.6 * smoothstep(0.22, 0.27, hb) * exp(-max(hb - 0.22, 0.0) * 10.0);
    float kick = mix(idleKick * 0.75, clamp(max(iAudioKick, iAudioBass * 0.75), 0.0, 1.0), live);
    float bass = mix(0.20, iAudioBass, live);
    float beat = mix(0.25, iAudioBeatStrength, live);
    float drop = live * iAudioDrop;
    // Cone excursion 0..1: forward on the kick, held by the sustained bass.
    float exc = clamp(0.80 * kick + 0.35 * bass, 0.0, 1.2);

    // Curated neon pairs, slowly alternating.
    float pick = smoothstep(0.35, 0.65, 0.5 + 0.5 * sin(iTime * 0.05));
    vec3 neonA = mix(vec3(0.85, 0.15, 1.00), vec3(1.00, 0.12, 0.45), pick);
    vec3 neonB = mix(vec3(0.10, 0.75, 1.00), vec3(0.20, 1.00, 0.70), pick);

    // Shake on hard kicks.
    vec2 shake = vec2(sin(iTime * 71.0), cos(iTime * 63.0)) * px * 0.9 * smoothstep(0.55, 1.0, kick);
    vec2 c = uv - vec2(0.0, -0.01) + shake;

    // Wall: neon wash from both sides.
    vec3 col = vec3(0.03, 0.02, 0.05);
    col += neonA * 0.30 * exp(-abs(uv.x + 0.75) * 3.0) * (0.6 + 0.6 * beat);
    col += neonB * 0.30 * exp(-abs(uv.x - 0.75) * 3.0) * (0.6 + 0.6 * beat);

    // Cabinet: carpeted box, lit from the upper left.
    vec2 box = abs(c) - vec2(0.45, 0.45);
    float cab = max(box.x, box.y);
    float carpet = 0.85 + 0.15 * hash21(floor(fragCoord * 0.5));
    vec3 cabCol = vec3(0.10, 0.10, 0.12) * carpet * (1.0 + 0.6 * dot(normalize(c + 1e-4), vec2(-0.6, 0.8)) * smoothstep(0.2, 0.6, length(c)));
    col = mix(col, cabCol, smoothstep(px, -px, cab));
    // Neon edge light.
    float edge = smoothstep(2.0 * px, 0.0, abs(cab));
    col += mix(neonA, neonB, step(0.0, c.x)) * edge * (0.22 + 0.10 * beat);

    float r = length(c);
    vec2 n = c / max(r, 1e-4);
    vec2 keyDir = normalize(vec2(-0.6, 0.8));
    float facing = dot(n, keyDir);  // +1 toward the light

    float R = 0.385;        // frame (basket) radius
    float Rs = 0.355;       // surround outer edge
    float Rc = 0.300;       // cone outer edge
    // LED ring between basket and cabinet.
    float ledRing = smoothstep(3.0 * px, 0.0, abs(r - (R + 0.020)));
    float ledAng = 0.5 - 0.5 * cos(atan(c.y, c.x) * 2.0 + iTime * 0.8 + 1.5 * beat);
    vec3 ledCol = mix(neonA, neonB, ledAng);
    col += ledCol * ledRing * (0.35 + 0.6 * bass + 0.3 * drop);
    col += ledCol * exp(-abs(r - (R + 0.02)) * 45.0) * (0.08 + 0.18 * bass + 0.15 * drop);

    if (r < R + 0.004) {
        // Steel basket rim.
        vec3 steel = vec3(0.30, 0.31, 0.34) * (0.55 + 0.55 * facing);
        steel += vec3(0.9) * pow(max(facing, 0.0), 12.0) * 0.6;

        // Rubber surround: a half-roll. Pushed out, the roll stretches and its
        // highlight rolls toward the cone.
        float s = clamp((r - Rc) / (Rs - Rc), 0.0, 1.0);
        float rollPeak = mix(0.55, 0.25, clamp(exc, 0.0, 1.0));
        float slope = (s < rollPeak) ? (s / rollPeak) : -((s - rollPeak) / (1.0 - rollPeak));
        float surroundLight = 0.5 + 0.45 * slope * facing;
        vec3 rubber = vec3(0.07, 0.07, 0.08) + vec3(0.32) * pow(max(surroundLight, 0.0), 3.0) * (0.6 + 0.6 * exc);

        // Cone: a shallow funnel. Shading follows the slope toward the light;
        // excursion steepens it and pushes the specular band outward.
        float conePos = r / Rc;  // 0 at the centre, 1 at the edge
        float depthSlope = 0.55 + 0.45 * exc;
        float lambert = 0.38 - 0.35 * facing * depthSlope;
        float specR = mix(0.48, 0.86, clamp(exc, 0.0, 1.0));
        float spec = exp(-pow((conePos - specR) / 0.10, 2.0)) * pow(max(-facing * 0.5 + 0.5, 0.0), 2.0);
        float grooves = 0.92 + 0.08 * sin(r * 260.0);
        vec3 coneCol = vec3(0.16, 0.16, 0.18) * (0.35 + lambert) * grooves;
        coneCol += vec3(0.85, 0.88, 1.0) * spec * (0.40 + 0.45 * exc);
        // Neon reflections on the cone from the side lights.
        coneCol += neonA * 0.20 * smoothstep(0.0, -0.9, n.x) * conePos;
        coneCol += neonB * 0.20 * smoothstep(0.0, 0.9, n.x) * conePos;
        // Mirrored spectrum shimmer across the cone (seam-safe angle mapping).
        float a = 0.5 - 0.5 * cos(atan(c.y, c.x));
        float specBand = live * sampleAudioSpectrum(iChannel0, 0.03 + a * 0.35);
        coneCol += mix(neonA, neonB, a) * specBand * 0.35 * smoothstep(1.0, 0.45, conePos) * smoothstep(0.30, 0.55, conePos);

        // Dust cap: a glossy dome that swells forward with the cone.
        float capR = 0.105 + 0.045 * exc;
        float capT = r / capR;
        vec2 m = c / capR;
        vec3 dome = vec3(m, sqrt(max(0.0, 1.0 - dot(m, m))));
        vec3 L = normalize(vec3(-0.5, 0.6, 0.65));
        float capDiff = max(dot(dome, L), 0.0);
        float capSpec = pow(max(dot(reflect(-L, dome), vec3(0.0, 0.0, 1.0)), 0.0), 24.0);
        vec3 capCol = vec3(0.05, 0.05, 0.06) + vec3(0.42, 0.42, 0.46) * capDiff * capDiff;
        capCol += vec3(1.0) * capSpec * (0.7 + 0.5 * exc);
        capCol += neonB * 0.25 * smoothstep(0.4, 1.0, capT) * smoothstep(0.0, 0.6, n.x);
        capCol += neonA * 0.25 * smoothstep(0.4, 1.0, capT) * smoothstep(0.0, 0.6, -n.x);

        vec3 driver = coneCol;
        driver = mix(driver, rubber, smoothstep(Rc - px, Rc + px, r));
        driver = mix(driver, steel, smoothstep(Rs - px, Rs + px, r));
        // Dark shadow ring where the cone meets the surround.
        driver *= 1.0 - 0.6 * exp(-pow((r - Rc) / (2.0 * px), 2.0));
        driver = mix(driver, capCol, smoothstep(capR + px, capR - px, r));
        // Soft shadow of the cap on the cone, opposite the light.
        driver *= 1.0 - 0.35 * smoothstep(capR + 0.04, capR, length(c + keyDir * 0.02)) * step(capR, r);

        col = mix(col, driver, smoothstep(R + px, R - px, r));
    }

    // Basket bolts.
    for (int i = 0; i < 8; ++i) {
        float ang = float(i) * TAU / 8.0 + 0.39;
        vec2 bp = vec2(cos(ang), sin(ang)) * (R - 0.014);
        float b = smoothstep(2.2 * px, 1.0 * px, length(c - bp));
        col = mix(col, vec3(0.55, 0.56, 0.60) * (0.6 + 0.6 * dot(vec2(cos(ang), sin(ang)), keyDir)), b);
    }

    // Pressure ripple in the air on each kick, travelling outward.
    // As the kick envelope decays the ripple travels outward and fades.
    float rippleR = R + 0.04 + 0.12 * clamp(1.0 - kick, 0.0, 1.0);
    float ripple = exp(-pow((r - rippleR) / (2.0 * px), 2.0)) * kick * 0.45 * step(R + 0.03, r) * smoothstep(0.50, 0.44, rippleR);
    col += mix(neonA, neonB, 0.5 + 0.5 * n.x) * ripple;

    // Dust thrown off the cone on drops.
    float motes = 0.0;
    for (int i = 0; i < 8; ++i) {
        float fi = float(i);
        float t = fract(iTime * 0.45 + hash21(vec2(fi, 3.1)));
        float ang = hash21(vec2(fi, 7.7)) * TAU;
        float dist = 0.10 + t * (0.18 + 0.30 * drop + 0.12 * kick);
        vec2 p = vec2(cos(ang), sin(ang)) * dist;
        motes += smoothstep(2.0 * px, 0.5 * px, length(c - p)) * (1.0 - t) * drop * step(0.3, drop);
    }
    col += vec3(0.9, 0.95, 1.0) * motes * 0.7;

    col = 1.0 - exp(-col * 1.6);
    fragColor = vec4(col, 1.0);
}
