/* led-matrix-shader
{
  "family": "aquarium",
  "tags": ["ambient", "scenic", "showcase", "organic", "calm", "soft", "depth"],
  "intensity": 0.28,
  "motion": 0.26,
  "music_affinity": 0.20,
  "performance_cost": 0.30,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

float hash(float n) { return fract(sin(n * 127.1) * 43758.5453); }
float aa;

float ellipse(vec2 p, vec2 radius) {
    return (length(p / radius) - 1.0) * min(radius.x, radius.y);
}
float coverage(float distance) { return 1.0 - smoothstep(-aa, aa, distance); }

vec3 fish(vec3 water, vec2 uv, vec2 center, float size, float direction, float seed, float depth) {
    vec2 p = (uv - center) / size;
    p.x *= direction;
    p.y += p.x * 0.055 * sin(iTime * 0.8 + seed);
    float tailBeat = sin(iTime * (3.4 + seed * 0.07) + seed * 2.0);
    float body = ellipse(p, vec2(0.78, 0.34));
    vec2 tail = p + vec2(0.86, -0.08 * tailBeat);
    float tailShape = max(max(-tail.x - 0.42, tail.x - 0.17), abs(tail.y) - (0.17 - tail.x) * 0.67);
    float dorsal = ellipse(p - vec2(-0.05, 0.26), vec2(0.45, 0.20));
    float ventral = ellipse(p - vec2(-0.13, -0.25), vec2(0.32, 0.16));
    vec3 tint = mix(vec3(0.12, 0.65, 0.68), vec3(0.95, 0.43, 0.09), step(0.5, fract(seed * .37)));
    vec3 fins = mix(water, tint * 0.65, 0.60);
    float finMask = coverage(min(tailShape, min(dorsal, ventral)) * size);
    water = mix(water, fins, finMask);
    float mask = coverage(body * size);
    float rounding = sqrt(max(0.0, 1.0 - dot(p / vec2(.78, .34), p / vec2(.78, .34))));
    float stripes = 0.85 + 0.15 * smoothstep(-0.2, 0.4, sin(p.x * 19.0 + seed));
    vec3 bodyColor = tint * (0.38 + 0.48 * rounding + 0.18 * p.y) * stripes;
    bodyColor += vec3(0.24, 0.29, 0.18) * exp(-pow((p.y - .16) * 15.0, 2.0)) * rounding;
    float gill = exp(-pow((p.x - .29) * 35.0, 2.0)) * (1.0 - smoothstep(.08, .30, abs(p.y)));
    bodyColor *= 1.0 - .20 * gill;
    bodyColor = mix(bodyColor, vec3(0.015, 0.15, 0.19), depth);
    water = mix(water, bodyColor, mask);
    float eye = coverage((length(p - vec2(.49, .08)) - .075) * size);
    water = mix(water, vec3(.012, .025, .025), eye * mask);
    float eyeLight = coverage((length(p - vec2(.51, .11)) - .025) * size);
    water = mix(water, vec3(.65, .76, .60), eyeLight * mask * .65);
    return water;
}

vec3 plants(vec3 color, vec2 uv, float foreground, float music) {
    for (int i = 0; i < 13; ++i) {
        float seed = float(i) + foreground * 37.0;
        float root = hash(seed + 4.0);
        float height = .13 + .32 * hash(seed + 8.0);
        height *= mix(.75, 1.0, foreground);
        float y = uv.y - .07;
        float fraction = clamp(y / height, 0.0, 1.0);
        float sway = (.010 + .022 * fraction) * sin(iTime * .43 + seed + fraction * 2.0);
        float center = root + sway * fraction + .07 * sin(seed) * fraction * fraction;
        float width = (.004 + .008 * hash(seed + 2.0)) * sin(fraction * 2.6 + .35);
        float d = max(abs(uv.x - center) - width, max(-y, y - height));
        vec3 blade = mix(vec3(.012, .15, .115), vec3(.12, .34, .14), fraction);
        blade *= .65 + foreground * .35 + .06 * music;
        float midrib = exp(-abs(uv.x - center) * 1100.0);
        blade += vec3(.04, .08, .025) * midrib;
        color = mix(color, blade, coverage(d) * mix(.55, .95, foreground));
    }
    return color;
}

vec3 leafyPlants(vec3 color, vec2 uv) {
    for (int plant = 0; plant < 3; ++plant) {
        float k = float(plant);
        float root = plant == 0 ? .075 : (plant == 1 ? .91 : .25);
        float height = plant == 2 ? .24 : .43;
        float y = uv.y - .075;
        float f = clamp(y / height, 0.0, 1.0);
        float stem = root + .020 * sin(iTime * .38 + k + f * 2.0) * f;
        float stemMask = coverage(max(abs(uv.x - stem) - .0025, max(-y, y - height)));
        color = mix(color, vec3(.09, .23, .075), stemMask);
        for (int leaf = 0; leaf < 7; ++leaf) {
            float j = float(leaf);
            float h = .045 + j * height / 8.0;
            float side = mod(j, 2.0) * 2.0 - 1.0;
            float lean = .020 * sin(iTime * .38 + k + h / height * 2.0) * h / height;
            vec2 center = vec2(root + lean + side * .022, .075 + h);
            vec2 q = uv - center;
            q = vec2(q.x * .90 + q.y * side * .44, -q.x * side * .44 + q.y * .90);
            float taper = 1.0 - .045 * j;
            float leafMask = coverage(ellipse(q, vec2(.041, .013) * taper));
            vec3 leafColor = mix(vec3(.025, .16, .10), vec3(.13, .32, .12), .5 + q.y * 23.0);
            leafColor += vec3(.045, .065, .016) * exp(-abs(q.y) * 750.0);
            color = mix(color, leafColor, leafMask);
        }
    }
    return color;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.y;
    float aspect = iResolution.x / iResolution.y;
    uv.x -= (aspect - 1.0) * .5;
    aa = .65 / iResolution.y;
    float live = step(.5, iAudioAvailable) * (1.0 - step(.5, iAudioSilence));
    float music = live * clamp(iAudioLoudness, 0.0, 1.0);
    vec3 color = mix(vec3(.008, .045, .060), vec3(.018, .24, .28), pow(uv.y, .8));
    float windowGlow = exp(-2.7 * pow((uv.x - .35) / .65, 2.0)) * pow(uv.y, 2.0);
    color += vec3(.075, .20, .16) * windowGlow;
    for (int i = 0; i < 4; ++i) {
        float k = float(i);
        float ray = uv.x - .10 - k * .22 + (1.0 - uv.y) * .25 + .025 * sin(iTime * .15 + k);
        color += vec3(.06, .15, .12) * exp(-pow(ray / .04, 2.0)) * (.15 + .85 * uv.y) * .24;
    }
    float sandLine = .11 + .015 * sin(uv.x * 7.0) + .008 * sin(uv.x * 19.0);
    float sand = coverage(uv.y - sandLine);
    float caustics = pow(.5 + .5 * sin(uv.x * 39.0 + sin(uv.y * 32.0 + iTime * .6) + iTime * .4), 5.0);
    vec3 sandColor = vec3(.15, .17, .105) * (.6 + .3 * uv.y / .12) + vec3(.04, .07, .035) * caustics;
    color = mix(color, sandColor, sand);
    color = plants(color, uv, 0.0, music);

    color = leafyPlants(color, uv);

    for (int i = 0; i < 10; ++i) {
        float k = float(i);
        float travel = fract(iTime * .009 + .035 * k + .26) * 1.7 - .35;
        float visitorAge = mod(iTime + 48.0, 73.0);
        float visitorX = -.30 + visitorAge * .08;
        float schoolTurn = sin(iTime * .035);
        float parting = exp(-pow((travel - visitorX) * 7.0, 2.0)) * .055;
        float y = .60 + .10 * sin(iTime * .10) + .045 * sin(k * 2.4) + .025 * sin(travel * 5.0 + schoolTurn);
        vec2 center = vec2(travel, y + parting * sin(k * 2.4));
        color = fish(color, uv, center, .021 + .003 * hash(k), 1.0, k + 21.0, .55);
    }
    for (int i = 0; i < 3; ++i) {
        float k = float(i);
        float direction = i == 1 ? -1.0 : 1.0;
        float x = fract(iTime * (.011 + .002 * k) + .26 + k * .21) * 1.5 - .25;
        if (direction < 0.0) x = 1.0 - x;
        float y = .34 + .15 * k + .035 * sin(iTime * .31 + k * 3.0);
        color = fish(color, uv, vec2(x, y), .067 + .010 * hash(k + 7.0), direction, k + 4.0, .08);
    }

    float visitorAge = mod(iTime + 48.0, 73.0);
    color = fish(color, uv, vec2(-.30 + visitorAge * .08, .57 + .04 * sin(visitorAge * .22)),
                 .098, 1.0, 5.0, .20);

    for (int i = 0; i < 3; ++i) {
        float k = float(i);
        vec2 center = vec2(.18 + .33 * k, .075);
        vec2 radius = vec2(.12 + .015 * sin(k), .062 + .018 * hash(k + 3.0));
        float rock = ellipse(uv - center, radius);
        float light = clamp((uv.y - center.y) / radius.y, 0.0, 1.0);
        vec3 rockColor = mix(vec3(.023, .065, .055), vec3(.15, .22, .13), light);
        rockColor *= .83 + .17 * sin(uv.x * 67.0 + sin(uv.y * 91.0));
        color = mix(color, rockColor, coverage(rock));
    }
    color = plants(color, uv, 1.0, music);

    for (int i = 0; i < 9; ++i) {
        float k = float(i);
        float age = mod(iTime + k * .33, 37.0);
        float visibility = smoothstep(0.0, .4, age) * (1.0 - smoothstep(6.5, 7.5, age));
        vec2 center = vec2(.78 + .018 * sin(age * 2.0 + k), .12 + age * .125);
        float radius = .006 + .003 * hash(k + 2.0);
        float d = length(uv - center);
        float bubble = exp(-pow((d - radius) / (aa * .8), 2.0));
        color += vec3(.11, .28, .27) * bubble * visibility * .55;
    }
    for (int i = 0; i < 14; ++i) {
        float k = float(i);
        vec2 mote = vec2(hash(k + 30.0) + .016 * sin(iTime * .12 + k), fract(hash(k + 60.0) + iTime * .003));
        float fade = smoothstep(.0, .10, mote.y) * (1.0 - smoothstep(.90, 1.0, mote.y));
        color += vec3(.06, .12, .10) * exp(-dot(uv - mote, uv - mote) * 65000.0) * fade;
    }
    color *= .85 + .15 * exp(-2.0 * dot(uv - .5, uv - .5));
    fragColor = vec4(1.0 - exp(-color * 1.9), 1.0);
}
