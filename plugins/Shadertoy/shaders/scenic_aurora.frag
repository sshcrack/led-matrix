/* led-matrix-shader
{
  "family": "aurora",
  "tags": ["ambient", "aurora", "scenic", "flow"],
  "intensity": 0.42,
  "motion": 0.48,
  "music_affinity": 0.10,
  "performance_cost": 0.25,
  "automatic_eligible": true,
  "audio_reactive": false
}
*/

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

vec3 auroraPalette(float t) {
    vec3 cyan = vec3(0.06, 0.78, 0.72);
    vec3 violet = vec3(0.54, 0.18, 0.90);
    vec3 rose = vec3(0.96, 0.24, 0.55);
    return mix(mix(cyan, violet, smoothstep(0.0, 0.62, t)), rose, smoothstep(0.58, 1.0, t));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec2 p = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;

    vec3 color = vec3(0.006, 0.010, 0.030);
    color += vec3(0.016, 0.022, 0.055) * (1.0 - uv.y);

    for (int i = 0; i < 4; ++i) {
        float fi = float(i);
        float phase = iTime * (0.14 + fi * 0.018) + fi * 1.73;
        float center = 0.06 + fi * 0.10
                     + 0.055 * sin(p.x * (2.2 + fi * 0.35) + phase)
                     + 0.022 * sin(p.x * 6.0 - phase * 1.7);
        float distanceToRibbon = abs(p.y - center);
        float width = 0.025 + fi * 0.004;
        float ribbon = exp(-distanceToRibbon / width);
        float curtain = 0.55 + 0.45 * sin(p.x * 4.0 + phase + sin(p.x * 2.0));
        color += auroraPalette(fract(0.17 * fi + uv.x * 0.28 + iTime * 0.018))
               * ribbon * curtain * (0.13 + 0.055 * fi);
    }

    vec2 starCell = floor((p + vec2(0.9, 0.55)) * 24.0);
    float star = step(0.965, hash21(starCell));
    vec2 starUv = fract((p + vec2(0.9, 0.55)) * 24.0) - 0.5;
    star *= exp(-42.0 * dot(starUv, starUv));
    star *= 0.55 + 0.45 * sin(iTime * 0.8 + hash21(starCell + 3.0) * 6.2831853);
    color += vec3(0.45, 0.68, 1.0) * star * 0.16;

    float horizon = exp(-22.0 * abs(p.y + 0.28));
    color += vec3(0.025, 0.08, 0.13) * horizon;

    color = 1.0 - exp(-color * 1.65);
    fragColor = vec4(color, 1.0);
}
