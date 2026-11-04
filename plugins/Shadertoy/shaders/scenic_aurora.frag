/* led-matrix-shader
{
  "family": "aurora",
  "tags": ["ambient", "aurora", "scenic", "showcase", "flow", "calm"],
  "intensity": 0.32,
  "motion": 0.25,
  "music_affinity": 0.10,
  "performance_cost": 0.27,
  "automatic_eligible": true,
  "audio_reactive": false
}
*/

#define TAU 6.28318530718

// Integer-cell friendly hash (the previous fract-product hash produced whole
// rows of identical values for integer cells, which lined stars up in rows).
float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
float hash11(float n) { return fract(sin(n * 127.1) * 43758.5453); }
float noise1(float x) {
    float i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(hash11(i), hash11(i + 1.0), f);
}

// Occasional substorm: roughly every 2.6 minutes the curtains brighten, move
// faster and grow a pink lower fringe for ~20 s, then settle again.
float substorm() {
    float age = mod(iTime + 70.0, 157.0);
    return smoothstep(0.0, 7.0, age) * (1.0 - smoothstep(14.0, 30.0, age));
}
// Extra curtain phase gained during substorms. Monotonic and continuous
// across period boundaries, so the curtains only ever speed up, never reverse.
float substormPhase() {
    float cycles = floor((iTime + 70.0) / 157.0);
    float age = mod(iTime + 70.0, 157.0);
    return 6.0 * (cycles + smoothstep(0.0, 30.0, age));
}

vec3 auroraPalette(float t) {
    vec3 green = vec3(0.06, 0.90, 0.47);
    vec3 violet = vec3(0.32, 0.22, 0.68);
    return mix(green, violet, smoothstep(.30, .95, .5 + .5 * sin(TAU * t)));
}

vec3 auroraSky(vec2 p) {
    vec2 uv = p + .5;

    vec3 color = vec3(0.003, 0.007, 0.026);
    color += vec3(0.012, 0.020, 0.060) * (0.42 + 0.58 * (1.0 - uv.y));
    float haze = 0.0;
    float storm = substorm();
    float stormPhase = substormPhase();

    for (int i = 0; i < 6; ++i) {
        float fi = float(i);
        float phase = iTime * (0.105 + fi * 0.014) + fi * 1.51 + stormPhase * (.6 + .1 * fi);
        // Spread the curtains over the full square panel instead of composing
        // them as a narrow widescreen ribbon across the middle.
        float center = 0.34 - fi * 0.125
                     + 0.070 * sin(p.x * (2.2 + fi * 0.24) + phase)
                     + 0.026 * sin(p.x * 5.8 - phase * 1.55 + fi);
        float d = p.y - center;
        float width = 0.031 + fi * 0.0045;
        float crest = exp(-d * d / (width * width * 1.55));
        float halo = exp(-abs(d) / (0.062 + fi * 0.004));

        // A soft curtain hangs below each crest. It gives the aurora body and
        // remains readable after the physical matrix's low-resolution sampling.
        float below = max(0.0, center - p.y);
        float curtain = exp(-below * (3.65 + fi * 0.16))
                      * (1.0 - smoothstep(0.00, 0.032, d));
        float folds = 0.58 + 0.42 * sin(p.x * (4.1 + fi * 0.31) + phase * 1.3 + sin(p.x * 1.7));
        folds *= folds;

        vec3 tint = auroraPalette(fract(0.13 * fi + uv.x * 0.24 + iTime * 0.014));
        float boost = 1.0 + .75 * storm;
        color += tint * crest * (0.16 + 0.035 * fi) * boost;
        color += tint * halo * (0.018 + 0.007 * fi) * boost;
        color += tint * curtain * folds * (0.021 + 0.009 * fi) * boost;
        // Pink nitrogen fringe along the lower edge during a substorm.
        float fringe = exp(-pow((d + width * 1.6) / width, 2.0)) * folds;
        color += vec3(.55, .10, .32) * fringe * storm * .38;
        haze += halo * 0.020;
    }

    color += vec3(0.09, 0.12, 0.28) * haze;

    // Two populations of stars: a few obvious anchors and a very dim fine field.
    vec2 starCell = floor((p + vec2(1.08, 0.57)) * vec2(21.0, 15.0));
    float seed = hash21(starCell);
    // Jitter each star inside its cell so the field does not read as a grid.
    vec2 jitter = (vec2(hash21(starCell + 7.31), hash21(starCell + 3.77)) - 0.5) * 0.55;
    vec2 starUv = fract((p + vec2(1.08, 0.57)) * vec2(21.0, 15.0)) - 0.5 - jitter;
    float star = step(0.955, seed) * exp(-38.0 * dot(starUv, starUv));
    float twinkle = 0.35 + 0.65 * sin(iTime * (0.34 + seed * 0.45) + seed * TAU);
    twinkle *= twinkle;
    color += mix(vec3(0.35, 0.58, 1.0), vec3(0.72, 0.82, 1.0), seed) * star * twinkle * 0.20;

    float horizon = exp(-24.0 * abs(p.y + 0.34));
    color += vec3(0.018, 0.075, 0.115) * horizon;
    color *= 0.84 + 0.16 * exp(-0.75 * dot(p, p));

    return color;
}


void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 p = (fragCoord - .5 * iResolution.xy) / iResolution.y;
    float aa = .65 / iResolution.y;
    float horizon = -.15;
    bool water = p.y < horizon;
    vec2 skyPoint = p;
    if (water) {
        skyPoint.y = 2.0 * horizon - p.y;
        skyPoint.x += .006 * sin(p.y * 170.0 + iTime * .7)
                    + .003 * sin(p.y * 390.0 - iTime * .4);
    }
    skyPoint.y = (skyPoint.y - horizon) * 1.45 - .43;
    vec3 color = auroraSky(skyPoint);
    if (water) {
        float ripple = .88 + .08 * sin(p.y * 230.0 + sin(p.x * 17.0) + iTime * .5)
                     + .04 * sin(p.y * 71.0 - p.x * 9.0 - iTime * .3);
        color *= .48 * ripple;
        color += vec3(.003, .011, .021);
    }
    // Two ranges with atmospheric perspective: a taller, hazier far range with
    // a snow-dusted peak, and a lower, darker near range.
    float yMirror = water ? 2.0 * horizon - p.y : p.y;
    vec3 auroraLight = auroraPalette(fract(.26 + (p.x + .5) * .24 + iTime * .014)) * (1.0 + .6 * substorm());
    for (int i = 0; i < 2; ++i) {
        float layer = float(i);
        float ridge;
        vec3 mountain;
        if (i == 0) {
            float peak = .085 * max(0.0, 1.0 - abs(p.x - .16) / .19) + .045 * max(0.0, 1.0 - abs(p.x + .30) / .14);
            ridge = horizon + .030 + peak + .040 * noise1(p.x * 7.0 + 3.0) + .014 * noise1(p.x * 19.0 + 7.0)
                  + .006 * noise1(p.x * 47.0);
            float height = ridge - horizon;
            float snow = smoothstep(.075, .115, height) * smoothstep(ridge - .035, ridge - .004, yMirror);
            snow *= .6 + .4 * noise1(p.x * 90.0);
            mountain = vec3(.012, .030, .046) + auroraLight * .018;
            mountain = mix(mountain, vec3(.05, .075, .10) + auroraLight * .07, snow);
        } else {
            ridge = horizon + .018 + .022 * noise1(p.x * 9.0 + 11.0) + .010 * noise1(p.x * 26.0 + 2.0)
                  + .004 * noise1(p.x * 61.0);
            mountain = vec3(.004, .011, .016) + auroraLight * .006;
        }
        float mask = 1.0 - smoothstep(ridge - aa, ridge + aa, yMirror);
        if (water) mountain *= .5;
        color = mix(color, mountain, mask);
    }
    // Irregular spruce line along the shore, with gaps.
    for (int i = 0; i < 30; ++i) {
        float k = float(i);
        float seed = hash21(vec2(k, 5.0));
        if (hash21(vec2(k, 9.0)) < .22) continue;
        float x = -.54 + k * .037 + .014 * (hash21(vec2(k, 2.0)) - .5);
        float height = .022 + .048 * seed * seed + .010 * hash21(vec2(k, 4.0));
        float y = yMirror - horizon;
        float width = (.010 + .008 * seed) * (1.0 - clamp(y / height, 0.0, 1.0));
        width *= .74 + .26 * sin(y * 520.0 + k);
        float d = max(abs(p.x - x) - width, max(-y, y - height));
        float mask = 1.0 - smoothstep(-aa, aa, d);
        color = mix(color, vec3(.002, .008, .010), mask);
    }
    float shore = -.50 + .08 * exp(-pow((p.x + .38) / .25, 2.0));
    color = mix(color, vec3(.002, .006, .008), 1.0 - smoothstep(shore - aa, shore + aa, p.y));
    fragColor = vec4(1.0 - exp(-color * 2.2), 1.0);
}
