/* led-matrix-shader
{
  "family": "organic",
  "tags": ["ambient", "organic", "plasma", "scenic", "calm"],
  "intensity": 0.48,
  "motion": 0.43,
  "music_affinity": 0.08,
  "performance_cost": 0.24,
  "automatic_eligible": true,
  "audio_reactive": false
}
*/

#define TAU 6.28318530718

vec3 gardenPalette(float t) {
    vec3 night = vec3(0.015, 0.020, 0.055);
    vec3 teal = vec3(0.05, 0.72, 0.62);
    vec3 violet = vec3(0.52, 0.16, 0.86);
    vec3 rose = vec3(0.92, 0.28, 0.52);
    vec3 a = mix(teal, violet, smoothstep(0.18, 0.62, t));
    return mix(mix(night, a, smoothstep(0.0, 0.24, t)), rose, smoothstep(0.72, 1.0, t));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float t = iTime * 0.16;

    vec2 p = uv;
    p += 0.055 * vec2(sin(p.y * 4.2 + t * 1.7), cos(p.x * 3.8 - t * 1.2));
    float field = 0.0;
    float glow = 0.0;

    for (int i = 0; i < 4; ++i) {
        float fi = float(i);
        vec2 center = vec2(
            sin(t * (0.72 + fi * 0.09) + fi * 1.71),
            cos(t * (0.61 + fi * 0.07) + fi * 2.13)
        ) * vec2(0.28, 0.19);
        float r = length(p - center);
        field += sin(r * (12.0 + fi * 1.8) - t * (2.0 + fi * 0.24)) / (1.0 + r * 8.0);
        glow += exp(-r * r * (18.0 + fi * 3.0)) * (0.34 + 0.08 * fi);
    }

    float petals = 0.5 + 0.5 * sin(field * 3.2 + p.x * 2.2 - p.y * 1.7);
    float veins = pow(0.5 + 0.5 * cos(field * 8.0 + length(p) * 17.0), 8.0);
    float vignette = smoothstep(0.78, 0.16, length(uv));
    vec3 color = gardenPalette(clamp(petals * 0.78 + glow * 0.24, 0.0, 1.0));
    color *= (0.10 + 0.62 * petals + 0.12 * veins + 0.24 * glow) * vignette;
    color = 1.0 - exp(-color * 1.35);
    fragColor = vec4(color, 1.0);
}
