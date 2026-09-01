/* led-matrix-shader
{
  "family": "organic",
  "tags": ["ambient", "organic", "plasma", "scenic", "calm", "showcase"],
  "intensity": 0.52,
  "motion": 0.44,
  "music_affinity": 0.08,
  "performance_cost": 0.26,
  "automatic_eligible": true,
  "audio_reactive": false
}
*/

#define TAU 6.28318530718

vec3 gardenPalette(float t) {
    vec3 night = vec3(0.010, 0.018, 0.050);
    vec3 teal = vec3(0.04, 0.82, 0.68);
    vec3 violet = vec3(0.50, 0.15, 0.94);
    vec3 rose = vec3(0.98, 0.26, 0.54);
    vec3 a = mix(teal, violet, smoothstep(0.16, 0.62, t));
    return mix(mix(night, a, smoothstep(0.0, 0.22, t)), rose, smoothstep(0.70, 1.0, t));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float t = iTime * 0.145;

    vec2 p = uv;
    p += 0.060 * vec2(sin(p.y * 3.9 + t * 1.7), cos(p.x * 3.6 - t * 1.25));
    float field = 0.0;
    float glow = 0.0;

    for (int i = 0; i < 5; ++i) {
        float fi = float(i);
        vec2 center = vec2(
            sin(t * (0.68 + fi * 0.075) + fi * 1.67),
            cos(t * (0.57 + fi * 0.065) + fi * 2.09)
        ) * vec2(0.37, 0.235);
        float r = length(p - center);
        field += sin(r * (10.5 + fi * 1.55) - t * (1.8 + fi * 0.20)) / (0.82 + r * 7.0);
        glow += exp(-r * r * (12.5 + fi * 2.2)) * (0.29 + 0.055 * fi);
    }

    float petals = 0.5 + 0.5 * sin(field * 3.0 + p.x * 1.85 - p.y * 1.45);
    float veins = pow(0.5 + 0.5 * cos(field * 7.2 + length(p) * 14.5), 9.0);
    float edge = 1.0 - smoothstep(0.20, 0.93, length(uv * vec2(0.82, 1.0)));
    float body = 0.10 + 0.60 * petals + 0.14 * veins + 0.27 * glow;

    vec3 color = vec3(0.0015, 0.004, 0.012);
    vec3 plasma = gardenPalette(clamp(petals * 0.74 + glow * 0.30, 0.0, 1.0));
    color += plasma * body * edge;

    // A broad colored breath around the organism stops it reading as a small
    // sticker in the center of a wide matrix.
    float aura = exp(-2.5 * dot(uv * vec2(0.72, 1.0), uv * vec2(0.72, 1.0)));
    color += gardenPalette(0.42 + 0.08 * sin(t)) * aura * glow * 0.045;
    color += vec3(0.010, 0.018, 0.045) * aura * 0.18;

    color *= 0.88 + 0.12 * sin(iTime * 0.31 + field * 0.4);
    color = 1.0 - exp(-color * 1.42);
    fragColor = vec4(color, 1.0);
}
