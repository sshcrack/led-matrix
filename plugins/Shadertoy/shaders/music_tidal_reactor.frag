/* led-matrix-shader
{
  "family": "organic",
  "tags": ["music", "organic", "depth", "audio-reactive", "showcase", "sculpture"],
  "intensity": 0.65,
  "motion": 0.55,
  "music_affinity": 0.95,
  "performance_cost": 0.55,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

float bass, mid, treble, surge, live;

mat2 turn(float angle) {
    return mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
}

vec3 objectSpace(vec3 p) {
    p.xz = turn(iTime * 0.18) * p.xz;
    p.yz = turn(iTime * 0.11) * p.yz;
    return p;
}

float membrane(vec3 p) {
    vec3 q = objectSpace(p);
    float scale = 2.65;
    vec3 cell = q * scale + vec3(0.0, 0.0, 0.20 * sin(iTime * 0.3));
    float gyroid = dot(sin(cell), cos(cell.yzx));
    float shell = (abs(gyroid) - (0.28 + 0.18 * mid)) / (scale * 3.0);
    float sphere = length(q) - (1.28 + 0.10 * bass + 0.06 * surge);
    return max(sphere, shell);
}

vec3 surfaceNormal(vec3 p) {
    vec2 e = vec2(0.002, -0.002);
    return normalize(e.xyy * membrane(p + e.xyy) + e.yyx * membrane(p + e.yyx)
                   + e.yxy * membrane(p + e.yxy) + e.xxx * membrane(p + e.xxx));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    bass = mix(0.22 + 0.04 * sin(iTime * 0.8), clamp(iAudioBass, 0.0, 1.0), live);
    mid = mix(0.24, clamp(iAudioMid, 0.0, 1.0), live);
    treble = mix(0.12, clamp(iAudioTreble, 0.0, 1.0), live);
    surge = live * clamp(iAudioDrop, 0.0, 1.0);
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    vec3 camera = vec3(0.0, 0.15, 4.5);
    vec3 ray = normalize(vec3(uv, -1.40));
    float projection = dot(camera, ray);
    float discriminant = projection * projection - dot(camera, camera) + 1.50 * 1.50;
    vec3 color = vec3(0.007, 0.012, 0.025);
    color += vec3(0.018, 0.06, 0.065) * exp(-6.0 * dot(uv, uv));
    if (discriminant > 0.0) {
        float start = max(0.0, -projection - sqrt(discriminant));
        float end = -projection + sqrt(discriminant);
        float distance = start;
        bool hit = false;
        float glow = 0.0;
        for (int i = 0; i < 80; ++i) {
            vec3 p = camera + distance * ray;
            float d = membrane(p);
            if (d < 0.0025) { hit = true; break; }
            if (distance > end) break;
            float stride = max(d * 0.85, 0.003);
            glow += exp(-5.0 * dot(p, p)) * stride;
            distance += stride;
        }
        if (hit) {
            vec3 p = camera + distance * ray;
            vec3 q = objectSpace(p);
            vec3 n = surfaceNormal(p);
            vec3 light = normalize(vec3(-2.0, 3.0, 4.0));
            float diffuse = max(0.0, dot(n, light));
            float rim = pow(1.0 - max(0.0, dot(n, -ray)), 2.5);
            float specular = pow(max(0.0, dot(n, normalize(light - ray))), 28.0);
            float occlusion = clamp(membrane(p + n * 0.14) / 0.14, 0.15, 1.0);
            float edge = smoothstep(1.10, 1.35, length(q));
            vec3 ceramic = mix(vec3(0.055, 0.30, 0.35), vec3(0.22, 0.68, 0.58), edge);
            color = ceramic * (0.16 + 0.9 * diffuse) * (0.45 + 0.55 * occlusion);
            color += vec3(0.60, 0.92, 0.86) * (specular * 0.5 + rim * 0.18);
            float flow = 0.5 + 0.5 * sin(q.y * 6.0 + q.x * 2.5 - iTime * 1.7);
            float veins = pow(flow, 10.0);
            float impulse = live * clamp(iAudioKick + 0.4 * iAudioSectionChange, 0.0, 1.0);
            color += vec3(1.0, 0.27, 0.045) * veins * (0.12 + 0.45 * impulse + 0.35 * surge);
            color += vec3(0.06, 0.20, 0.27) * rim * treble;
        }
        float closest = max(0.0, -projection);
        vec3 corePoint = camera + closest * ray;
        float coreGlow = exp(-11.0 * dot(corePoint, corePoint));
        if (!hit) color += vec3(1.0, 0.25, 0.045) * coreGlow * (0.6 + 0.35 * bass + surge * 0.4);
        color += vec3(1.0, 0.23, 0.035) * glow * (0.4 + 0.4 * surge);
    }
    color *= 1.0 - 0.35 * smoothstep(0.3, 0.75, length(uv));
    color = pow(1.0 - exp(-color * 1.8), vec3(0.9));
    fragColor = vec4(color, 1.0);
}
