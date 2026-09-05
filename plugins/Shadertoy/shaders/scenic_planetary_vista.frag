/* led-matrix-shader
{
  "family": "planetary",
  "tags": ["ambient", "scenic", "showcase", "space", "calm", "soft", "depth"],
  "intensity": 0.30,
  "motion": 0.18,
  "music_affinity": 0.15,
  "performance_cost": 0.30,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

float hash(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float noise(vec3 p) {
    vec3 cell = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(cell), hash(cell + vec3(1,0,0)), f.x),
                   mix(hash(cell + vec3(0,1,0)), hash(cell + vec3(1,1,0)), f.x), f.y),
               mix(mix(hash(cell + vec3(0,0,1)), hash(cell + vec3(1,0,1)), f.x),
                   mix(hash(cell + vec3(0,1,1)), hash(cell + vec3(1,1,1)), f.x), f.y), f.z);
}
float cloud(vec3 p) {
    return .55 * noise(p) + .28 * noise(p * 2.03) + .17 * noise(p * 4.07);
}
float sphereHit(vec3 origin, vec3 ray, vec3 center, float radius) {
    vec3 q = origin - center;
    float b = dot(q, ray);
    float discriminant = b * b - dot(q, q) + radius * radius;
    return discriminant < 0.0 ? 1000.0 : -b - sqrt(discriminant);
}
float segment(vec2 p, vec2 a, vec2 b) {
    vec2 q = p - a, v = b - a;
    return length(q - v * clamp(dot(q, v) / dot(v, v), 0.0, 1.0));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 p = (fragCoord - .5 * iResolution.xy) / iResolution.y;
    float aa = .75 / iResolution.y;
    float live = step(.5, iAudioAvailable) * (1.0 - step(.5, iAudioSilence));
    float music = live * clamp(iAudioLoudness, 0.0, 1.0);
    vec3 color = vec3(.006, .008, .022);
    float nebula = cloud(vec3(p * 3.0, iTime * .006));
    float band = exp(-pow((p.y - p.x * .4 + .15) * 3.3, 2.0));
    color += mix(vec3(.014, .018, .04), vec3(.055, .024, .048), nebula) * band * nebula;
    for (int i = 0; i < 45; ++i) {
        float k = float(i);
        vec2 star = vec2(hash(vec3(k, 1, 4)), hash(vec3(k, 7, 3))) - .5;
        float size = mix(.0014, .003, hash(vec3(k, 9, 1)));
        float brightness = .32 + .12 * sin(iTime * (.18 + .1 * hash(vec3(k))) + k);
        float d = length(p - star);
        float disc = 1.0 - smoothstep(size, size + aa, d);
        color += mix(vec3(.38, .51, .68), vec3(.76, .62, .43), hash(vec3(k, 8, 2))) * disc * brightness;
    }

    vec3 camera = vec3(0.0, 0.0, 6.2);
    vec3 ray = normalize(vec3(p, -1.55));
    vec3 center = vec3(.15, .50, 0.0);
    vec3 light = normalize(vec3(-.65, .8, 1.25));
    float radius = 1.01;
    float planet = sphereHit(camera, ray, center, radius);

    vec2 moonPosition = p - vec2(-.33, .37);
    float moonRadius = .047;
    if (length(moonPosition) < moonRadius) {
        vec2 m = moonPosition / moonRadius;
        vec3 n = vec3(m, sqrt(max(0.0, 1.0 - dot(m, m))));
        float shade = max(0.0, dot(n, normalize(vec3(-.8, .3, .4))));
        float terrain = .7 + .3 * cloud(n * 7.0);
        color = vec3(.20, .34, .43) * (shade * terrain + .025);
    }

    vec3 ringNormal = normalize(vec3(.25, .77, .55));
    float denominator = dot(ray, ringNormal);
    float ringDistance = 1000.0;
    float ringOpacity = 0.0;
    vec3 ringColor = vec3(0.0);
    if (abs(denominator) > .001) {
        ringDistance = dot(center - camera, ringNormal) / denominator;
        vec3 ringPoint = camera + ray * ringDistance - center;
        float r = length(ringPoint);
        float edge = smoothstep(1.22, 1.26, r) * (1.0 - smoothstep(1.85, 1.91, r));
        float cassini = 1.0 - .8 * exp(-pow((r - 1.61) / .026, 2.0));
        float dust = .7 + .14 * sin(r * 110.0) + .10 * sin(r * 47.0);
        float shadeDistance = sphereHit(center + ringPoint + light * .01, light, center, radius);
        float shadow = shadeDistance > 0.0 && shadeDistance < 10.0 ? .20 : 1.0;
        ringOpacity = edge * cassini * dust * .80;
        ringColor = mix(vec3(.31, .23, .15), vec3(.56, .45, .30), (1.0 - smoothstep(1.25, 1.85, r)));
        ringColor *= shadow;
        if (ringDistance > 0.0 && ringDistance > planet)
            color = mix(color, ringColor, ringOpacity);
    }

    vec3 relative = camera - center;
    float closest = max(0.0, -dot(relative, ray));
    float limbDistance = length(relative + ray * closest);
    float atmosphere = exp(-max(0.0, limbDistance - radius) * 52.0);
    float sunSide = max(.0, dot(normalize(relative + ray * closest), light));
    color += vec3(.12, .20, .30) * atmosphere * sunSide * (.26 + .02 * music);

    if (planet < 100.0) {
        vec3 n = normalize(camera + ray * planet - center);
        float rotation = iTime * .015;
        vec3 q = vec3(n.x * cos(rotation) + n.z * sin(rotation), n.y,
                      -n.x * sin(rotation) + n.z * cos(rotation));
        float turbulence = cloud(q * 6.0 + vec3(iTime * .008, 0.0, 0.0));
        float stripes = .5 + .5 * sin(q.y * 34.0 + turbulence * 3.5);
        float broadBands = .5 + .5 * sin(q.y * 13.0 + turbulence);
        vec3 surface = mix(vec3(.40, .24, .13), vec3(.76, .62, .40), stripes * .55 + broadBands * .45);
        float diffuse = smoothstep(-.08, .85, dot(n, light));
        float limb = pow(1.0 - max(0.0, dot(n, -ray)), 3.0);
        color = surface * (diffuse * .82 + .035);
        color += vec3(.13, .23, .34) * limb * diffuse * .40;
        color *= 1.0 - .10 * exp(-pow((n.y + .15) * 24.0, 2.0));
    }
    if (ringDistance > 0.0 && ringDistance < planet)
        color = mix(color, ringColor, ringOpacity);

    float event = mod(iTime + 9.0, 53.0);
    float fade = smoothstep(0.0, .7, event) * (1.0 - smoothstep(2.0, 3.0, event));
    vec2 meteor = vec2(-.47 + event * .24, .45 - event * .10);
    float trail = exp(-segment(p, meteor, meteor + vec2(-.095, .04)) * 600.0);
    color += vec3(.28, .38, .48) * trail * fade;

    for (int i = 0; i < 3; ++i) {
        float layer = float(i);
        float ridge = -.29 - .073 * layer + .026 * sin(p.x * (9.0 + layer * 4.0) + layer * 2.0)
                    + .012 * sin(p.x * 29.0 + layer) + .008 * sin(p.x * 57.0);
        float mountain = 1.0 - smoothstep(ridge - aa, ridge + aa, p.y);
        vec3 rock = mix(vec3(.046, .065, .10), vec3(.010, .017, .026), layer * .5);
        rock += vec3(.024, .028, .033) * exp(-max(0.0, ridge - p.y) * 60.0);
        rock *= 1.0 + .035 * music;
        color = mix(color, rock, mountain);
    }
    color *= .90 + .10 * exp(-2.0 * dot(p, p));
    fragColor = vec4(1.0 - exp(-color * 1.75), 1.0);
}
