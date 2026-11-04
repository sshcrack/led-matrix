/* led-matrix-shader
{
  "family": "planetary",
  "tags": ["ambient", "scenic", "showcase", "space", "calm", "soft", "depth"],
  "intensity": 0.30,
  "motion": 0.18,
  "music_affinity": 0.15,
  "performance_cost": 0.32,
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
// Ring opacity and colour at radius r (planet radii, ring plane).
float ringOpacityAt(float r) {
    float edge = smoothstep(1.22, 1.26, r) * (1.0 - smoothstep(1.85, 1.91, r));
    float cassini = 1.0 - .8 * exp(-pow((r - 1.61) / .026, 2.0));
    float dust = .7 + .14 * sin(r * 110.0) + .10 * sin(r * 47.0);
    return edge * cassini * dust * .80;
}

// Layered terrain height (screen space).
float ridgeAt(float x, float layer) {
    float n = noise(vec3(x * (3.2 + 2.0 * layer) + layer * 5.0, layer * 7.0, 0.0));
    return -.33 - .060 * layer
         + .085 * n * n * (1.0 - .25 * layer)
         + .020 * noise(vec3(x * (13.0 + 5.0 * layer), layer * 3.0, 1.0))
         + .007 * noise(vec3(x * 44.0, layer, 2.0));
}

// The small moon orbits roughly every 5 minutes on an ellipse tilted with the
// rings: it passes behind the planet, then transits in front of it.
const float MOON_PERIOD = 300.0;
vec3 moonOrbit() {
    float a = iTime * 6.28318530718 / MOON_PERIOD + 2.3;
    vec2 local = vec2(cos(a) * .44, sin(a) * .29);
    float tilt = -.10;
    vec2 offset = vec2(local.x * cos(tilt) - local.y * sin(tilt), local.x * sin(tilt) + local.y * cos(tilt));
    // z > 0: on the far side of the planet.
    return vec3(vec2(.0375, .125) + offset, sin(a));
}

vec3 drawMoon(vec3 color, vec2 p, vec3 orbit, vec3 light, float aa) {
    float moonRadius = .043 * (1.0 - .12 * orbit.z);
    vec2 moonPosition = p - orbit.xy;
    float dist = length(moonPosition);
    if (dist < moonRadius + aa) {
        vec2 m = moonPosition / moonRadius;
        vec3 n = vec3(m, sqrt(max(0.0, 1.0 - dot(m, m))));
        float shade = smoothstep(-.05, .6, dot(n, light));
        float terrain = .7 + .3 * cloud(n * 7.0 + 3.0);
        vec3 moon = vec3(.20, .34, .43) * (shade * terrain + .025);
        color = mix(color, moon, 1.0 - smoothstep(moonRadius - aa, moonRadius + aa, dist));
    }
    return color;
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

    vec3 orbit = moonOrbit();
    if (orbit.z > 0.0) color = drawMoon(color, p, orbit, light, aa);

    vec3 ringNormal = normalize(vec3(.25, .77, .55));
    float denominator = dot(ray, ringNormal);
    float ringDistance = 1000.0;
    float ringOpacity = 0.0;
    vec3 ringColor = vec3(0.0);
    if (abs(denominator) > .001) {
        ringDistance = dot(center - camera, ringNormal) / denominator;
        vec3 ringPoint = camera + ray * ringDistance - center;
        float r = length(ringPoint);
        float shadeDistance = sphereHit(center + ringPoint + light * .01, light, center, radius);
        float shadow = shadeDistance > 0.0 && shadeDistance < 10.0 ? .20 : 1.0;
        ringOpacity = ringOpacityAt(r);
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
        // Shadow of the rings on the cloud tops.
        vec3 hit = camera + ray * planet;
        float toward = dot(light, ringNormal);
        if (abs(toward) > .001) {
            float t = dot(center - hit, ringNormal) / toward;
            if (t > 0.0) color *= 1.0 - .72 * ringOpacityAt(length(hit + light * t - center));
        }
    }
    if (ringDistance > 0.0 && ringDistance < planet)
        color = mix(color, ringColor, ringOpacity);
    if (orbit.z <= 0.0) color = drawMoon(color, p, orbit, light, aa);

    float event = mod(iTime + 9.0, 53.0);
    float fade = smoothstep(0.0, .7, event) * (1.0 - smoothstep(2.0, 3.0, event));
    vec2 meteor = vec2(-.47 + event * .24, .45 - event * .10);
    float trail = exp(-segment(p, meteor, meteor + vec2(-.095, .04)) * 600.0);
    color += vec3(.28, .38, .48) * trail * fade;

    // Layered terrain: hazier far ridges, darker near ones, and a warm rim of
    // planetshine on slopes that face the light.
    vec2 light2 = normalize(vec2(-.65, .8));
    for (int i = 0; i < 3; ++i) {
        float layer = float(i);
        float ridge = ridgeAt(p.x, layer);
        float slope = (ridgeAt(p.x + .004, layer) - ridgeAt(p.x - .004, layer)) / .008;
        float facing = max(0.0, dot(normalize(vec2(-slope, 1.0)), light2));
        float mountain = 1.0 - smoothstep(ridge - aa, ridge + aa, p.y);
        float depthBelow = max(0.0, ridge - p.y);
        vec3 rock = mix(vec3(.050, .066, .10), vec3(.009, .014, .022), layer * .5);
        rock += vec3(.020, .026, .034) * exp(-depthBelow * 60.0) * (1.0 - .5 * layer);
        rock += vec3(.16, .11, .065) * exp(-depthBelow * 70.0) * pow(facing, 3.0) * (.55 + .35 * layer);
        rock *= .92 + .08 * noise(vec3(p * 60.0, layer));
        rock *= 1.0 + .035 * music;
        // Thin mist pooling in the valley behind each nearer ridge.
        color += vec3(.020, .026, .040) * exp(-max(0.0, p.y - ridge) * 40.0) * (1.0 - mountain) * step(.5, layer);
        color = mix(color, rock, mountain);
    }
    color *= .90 + .10 * exp(-2.0 * dot(p, p));
    fragColor = vec4(1.0 - exp(-color * 1.75), 1.0);
}
