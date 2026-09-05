/* led-matrix-shader
{
  "family": "sculpture",
  "tags": ["music", "depth", "geometric", "audio-reactive", "showcase", "cinematic"],
  "intensity": 0.72,
  "motion": 0.62,
  "music_affinity": 1.0,
  "performance_cost": 0.48,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

#define TAU 6.28318530718

float bass, mid, treble, impact, opening;

mat2 rotation(float a) {
    return mat2(cos(a), -sin(a), sin(a), cos(a));
}

vec3 metalPalette(float height) {
    return mix(vec3(0.10, 0.42, 0.68), vec3(0.66, 0.16, 0.38), smoothstep(-1.0, 1.5, height));
}

vec2 sculpture(vec3 p) {
    vec3 q = p;
    q.xz = rotation(0.19 * iTime) * q.xz;
    q.xy = rotation(0.22 * sin(iTime * 0.23)) * q.xy;
    float crystal = (dot(abs(q), vec3(1.0)) - (0.68 + 0.10 * bass)) * 0.57735;
    vec2 result = vec2(crystal, 1.0);

    float angle = atan(p.z, p.x);
    float sector = TAU / 6.0;
    float folded = mod(angle + sector * 0.5, sector) - sector * 0.5;
    vec3 rib = vec3(cos(folded) * length(p.xz), p.y, sin(folded) * length(p.xz));
    float height = clamp(rib.y, -1.35, 1.35);
    float spread = 0.93 + 0.38 * opening + 0.12 * bass;
    float bow = spread + 0.34 * cos(height * 1.45);
    float ribs = length(vec3(rib.x - bow, rib.y - height, rib.z)) - (0.10 + 0.035 * mid);
    if (ribs < result.x) result = vec2(ribs, 2.0);

    vec3 ring = p;
    ring.xy = rotation(0.22 * sin(iTime * 0.31)) * ring.xy;
    float halo = length(vec2(length(ring.xz) - (1.12 + 0.20 * opening), abs(ring.y) - 1.38)) - 0.055;
    if (halo < result.x) result = vec2(halo, 3.0);
    return result;
}

vec3 normalAt(vec3 p) {
    vec2 e = vec2(0.002, -0.002);
    return normalize(e.xyy * sculpture(p + e.xyy).x + e.yyx * sculpture(p + e.yyx).x
                   + e.yxy * sculpture(p + e.yxy).x + e.xxx * sculpture(p + e.xxx).x);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    bass = mix(0.18 + 0.05 * sin(iTime * 0.7), clamp(iAudioBass, 0.0, 1.0), live);
    mid = mix(0.20, clamp(iAudioMid, 0.0, 1.0), live);
    treble = mix(0.10, clamp(iAudioTreble, 0.0, 1.0), live);
    impact = live * clamp(iAudioDrop, 0.0, 1.0);
    opening = mix(0.35 + 0.20 * sin(iTime * 0.17),
                  clamp(0.22 + iAudioLoudness * 0.5 + iAudioEnergyTrend * 0.25 + impact * 0.3, 0.0, 1.0), live);

    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float azimuth = iTime * 0.13;
    vec3 camera = vec3(5.3 * sin(azimuth), 2.4 + 0.25 * sin(iTime * 0.21), 5.3 * cos(azimuth));
    vec3 forward = normalize(vec3(0.0, -0.05, 0.0) - camera);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(right, forward);
    vec3 ray = normalize(forward * 1.65 + right * uv.x + up * uv.y);

    vec3 color = vec3(0.006, 0.009, 0.025) + vec3(0.02, 0.025, 0.055) * exp(-3.0 * dot(uv, uv));
    float distance = 0.0;
    vec2 surface = vec2(1.0, 0.0);
    float glow = 0.0;
    for (int stepIndex = 0; stepIndex < 72; ++stepIndex) {
        vec3 p = camera + ray * distance;
        surface = sculpture(p);
        float epsilon = max(0.002, distance * 0.0006);
        if (surface.x < epsilon || distance > 10.0) break;
        float stride = max(surface.x * 0.65, 0.003);
        glow += exp(-18.0 * abs(surface.x)) * stride;
        distance += stride;
    }

    if (distance < 10.0 && surface.x < max(0.002, distance * 0.0006)) {
        vec3 p = camera + ray * distance;
        vec3 n = normalAt(p);
        vec3 light = normalize(vec3(-3.0, 4.0, 3.0));
        float diffuse = max(0.0, dot(n, light));
        float rim = pow(1.0 - max(0.0, dot(n, -ray)), 3.0);
        float specular = pow(max(0.0, dot(n, normalize(light - ray))), 32.0);
        vec3 base = metalPalette(p.y);
        color = base * (0.16 + 0.85 * diffuse) + vec3(0.6, 0.8, 1.0) * specular * 0.55;
        color += vec3(0.10, 0.65, 0.88) * rim * 0.55;
        if (surface.y < 1.5) {
            float facets = 0.5 + 0.5 * sin(p.y * 9.0 - iTime * 1.8);
            color = vec3(1.0, 0.36, 0.09) * (0.35 + diffuse * 0.65 + facets * 0.16);
            color += vec3(1.0, 0.65, 0.25) * rim * (0.5 + impact);
        } else if (surface.y > 2.5) {
            color = vec3(0.25, 0.72, 1.0) * (0.5 + 0.5 * treble);
        } else {
            float pulse = pow(0.5 + 0.5 * sin(p.y * 5.0 - iTime * 2.1), 8.0);
            color += vec3(0.30, 0.75, 1.0) * pulse * (0.10 + live * iAudioKick * 0.6);
        }
    } else if (ray.y < -0.001) {
        float floorDistance = (-1.65 - camera.y) / ray.y;
        vec3 floorPoint = camera + floorDistance * ray;
        float radius = length(floorPoint.xz);
        float pool = exp(-0.60 * radius * radius);
        float rings = exp(-26.0 * abs(radius - (1.75 + 0.12 * bass)));
        color += vec3(0.055, 0.16, 0.28) * pool;
        color += vec3(0.12, 0.48, 0.70) * rings * 0.28;
    }
    color += vec3(0.12, 0.42, 0.65) * min(glow, 0.65) * 0.7;
    color *= 1.0 - 0.35 * smoothstep(0.30, 0.80, length(uv));
    color = pow(1.0 - exp(-color * 1.5), vec3(0.92));
    fragColor = vec4(color, 1.0);
}
