/* led-matrix-shader
{
  "family": "aurora",
  "tags": ["ambient", "aurora", "scenic", "flow", "calm", "showcase"],
  "intensity": 0.48,
  "motion": 0.48,
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
    vec3 cyan = vec3(0.05, 0.90, 0.74);
    vec3 violet = vec3(0.45, 0.18, 0.96);
    vec3 rose = vec3(1.00, 0.24, 0.58);
    return mix(mix(cyan, violet, smoothstep(0.0, 0.58, t)), rose, smoothstep(0.56, 1.0, t));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec2 p = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;

    vec3 color = vec3(0.003, 0.007, 0.026);
    color += vec3(0.012, 0.020, 0.060) * (0.42 + 0.58 * (1.0 - uv.y));
    float haze = 0.0;

    for (int i = 0; i < 5; ++i) {
        float fi = float(i);
        float phase = iTime * (0.105 + fi * 0.014) + fi * 1.51;
        float center = 0.245 - fi * 0.088
                     + 0.060 * sin(p.x * (2.0 + fi * 0.25) + phase)
                     + 0.022 * sin(p.x * 5.4 - phase * 1.55 + fi);
        float d = p.y - center;
        float width = 0.027 + fi * 0.004;
        float crest = exp(-d * d / (width * width * 1.55));
        float halo = exp(-abs(d) / (0.062 + fi * 0.004));

        // A soft curtain hangs below each crest. It gives the aurora body and
        // remains readable after the physical matrix's low-resolution sampling.
        float below = max(0.0, center - p.y);
        float curtain = exp(-below * (4.6 + fi * 0.18))
                      * (1.0 - smoothstep(0.00, 0.032, d));
        float folds = 0.58 + 0.42 * sin(p.x * (4.1 + fi * 0.31) + phase * 1.3 + sin(p.x * 1.7));
        folds *= folds;

        vec3 tint = auroraPalette(fract(0.13 * fi + uv.x * 0.24 + iTime * 0.014));
        color += tint * crest * (0.16 + 0.035 * fi);
        color += tint * halo * (0.018 + 0.007 * fi);
        color += tint * curtain * folds * (0.018 + 0.010 * fi);
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

    color = 1.0 - exp(-color * 1.72);
    fragColor = vec4(color, 1.0);
}
