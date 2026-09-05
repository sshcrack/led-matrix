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

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
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

    for (int i = 0; i < 6; ++i) {
        float fi = float(i);
        float phase = iTime * (0.105 + fi * 0.014) + fi * 1.51;
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
        color += tint * crest * (0.16 + 0.035 * fi);
        color += tint * halo * (0.018 + 0.007 * fi);
        color += tint * curtain * folds * (0.021 + 0.009 * fi);
        haze += halo * 0.020;
    }

    color += vec3(0.09, 0.12, 0.28) * haze;

    // Two populations of stars: a few obvious anchors and a very dim fine field.
    vec2 starCell = floor((p + vec2(1.08, 0.57)) * vec2(21.0, 15.0));
    float seed = hash21(starCell);
    vec2 starUv = fract((p + vec2(1.08, 0.57)) * vec2(21.0, 15.0)) - 0.5;
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
    for (int i = 0; i < 2; ++i) {
        float layer = float(i);
        float ridge = horizon + .035 + .018 * layer
                    + .024 * sin(p.x * 14.0 + layer * 3.0)
                    + .018 * sin(p.x * 27.0 + layer)
                    + .012 * sin(p.x * 51.0);
        float y = water ? 2.0 * horizon - p.y : p.y;
        float mask = 1.0 - smoothstep(ridge - aa, ridge + aa, y);
        vec3 mountain = layer == 0 ? vec3(.009, .027, .039) : vec3(.005, .015, .021);
        if (water) mountain *= .5;
        color = mix(color, mountain, mask);
    }
    for (int i = 0; i < 24; ++i) {
        float k = float(i);
        float x = -.52 + k * .045;
        float seed = hash21(vec2(k, 5.0));
        float height = .035 + .035 * seed;
        float y = (water ? 2.0 * horizon - p.y : p.y) - horizon;
        float width = .016 * (1.0 - clamp(y / height, 0.0, 1.0));
        width *= .78 + .22 * sin(y * 500.0);
        float d = max(abs(p.x - x) - width, max(-y, y - height));
        float mask = 1.0 - smoothstep(-aa, aa, d);
        color = mix(color, vec3(.002, .008, .010), mask);
    }
    float shore = -.50 + .08 * exp(-pow((p.x + .38) / .25, 2.0));
    color = mix(color, vec3(.002, .006, .008), 1.0 - smoothstep(shore - aa, shore + aa, p.y));
    fragColor = vec4(1.0 - exp(-color * 2.2), 1.0);
}
