/* led-matrix-shader
{
  "family": "aquarium",
  "tags": ["ambient", "scenic", "showcase", "organic", "calm", "soft", "depth"],
  "intensity": 0.28,
  "motion": 0.26,
  "music_affinity": 0.20,
  "performance_cost": 0.32,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

// A planted aquarium seen through the front glass.
//  - water surface with a shimmering underside; light shafts hang from it
//  - half-buried, top-lit rocks with moss in a sand bed
//  - three species: cichlids, an angelfish and a schooling group of neon tetras
//  - fish wander, slow down and turn (side profile narrows through the turn);
//    the school turns as a ripple because every member lags the leader a bit
//  - sparse events: a large visitor crosses roughly every 73 s and the school
//    parts around it; roughly every 3.5 min food flakes sink from the surface
//    and the foreground fish rise to feed
// Every motion is continuous in time (no visible wraps) so it passes the
// temporal smoke test and stays calm on the matrix.

#define SPECIES_CICHLID 0
#define SPECIES_TETRA 1
#define SPECIES_ANGEL 2

float hash(float n) { return fract(sin(n * 127.1) * 43758.5453); }
float aa;

float ellipse(vec2 p, vec2 radius) {
    return (length(p / radius) - 1.0) * min(radius.x, radius.y);
}
float coverage(float distance) { return 1.0 - smoothstep(-aa, aa, distance); }

mat2 rot(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}

// ---------------------------------------------------------------- feeding event
const float FEED_PERIOD = 211.0;
float feedAge() { return mod(iTime + 140.0, FEED_PERIOD); }
float feedingAmount() {
    float age = feedAge();
    return smoothstep(4.0, 11.0, age) * (1.0 - smoothstep(24.0, 36.0, age));
}

// ---------------------------------------------------------------- visitor
const float VISITOR_PERIOD = 73.0;
float visitorAge() { return mod(iTime + 48.0, VISITOR_PERIOD); }
float visitorX() { return -.30 + visitorAge() * .08; }

// ---------------------------------------------------------------- fish
// facing: +1 swims right, -1 swims left, values near 0 are mid-turn.
vec3 fish(vec3 water, vec2 uv, vec2 center, float size, float facing, float seed, float depth, int species) {
    vec2 p = (uv - center) / size;
    float dir = facing < 0.0 ? -1.0 : 1.0;
    // Narrow the side profile while turning; never collapse fully so the fish
    // stays visible as a slim silhouette when it faces the viewer.
    p.x *= dir / mix(.24, 1.0, abs(facing));
    p.y += p.x * 0.055 * sin(iTime * 0.8 + seed);
    float tailBeat = sin(iTime * (3.4 + seed * 0.07) + seed * 2.0);

    vec2 bodyR;
    vec3 tint;
    float finD;
    if (species == SPECIES_TETRA) {
        bodyR = vec2(.80, .25);
        tint = vec3(.16, .22, .30);
        vec2 tail = p + vec2(.88, -.07 * tailBeat);
        float tailShape = max(max(-tail.x - .30, tail.x - .14), abs(tail.y) - (.14 - tail.x) * .60);
        float dorsal = ellipse(p - vec2(-.12, .20), vec2(.24, .10));
        finD = min(tailShape, dorsal);
    } else if (species == SPECIES_ANGEL) {
        bodyR = vec2(.52, .46);
        tint = vec3(.56, .58, .50);
        vec2 tail = p + vec2(.56, -.06 * tailBeat);
        float tailShape = max(max(-tail.x - .36, tail.x - .12), abs(tail.y) - (.24 - tail.x) * .80);
        // Long swept-back dorsal and ventral fins that trail with the swim beat.
        vec2 dp = rot(-.62 + .05 * tailBeat) * (p - vec2(-.22, .52));
        vec2 vp = rot(.62 - .05 * tailBeat) * (p - vec2(-.22, -.52));
        float dorsal = ellipse(dp, vec2(.14, .48));
        float ventral = ellipse(vp, vec2(.12, .50));
        finD = min(tailShape, min(dorsal, ventral));
    } else {
        bodyR = vec2(.78, .34);
        tint = mix(vec3(0.12, 0.65, 0.68), vec3(0.95, 0.43, 0.09), step(0.5, fract(seed * .37)));
        vec2 tail = p + vec2(0.86, -0.08 * tailBeat);
        float tailShape = max(max(-tail.x - 0.42, tail.x - 0.17), abs(tail.y) - (0.17 - tail.x) * 0.67);
        float dorsal = ellipse(p - vec2(-0.05, 0.26), vec2(0.45, 0.20));
        float ventral = ellipse(p - vec2(-0.13, -0.25), vec2(0.32, 0.16));
        finD = min(tailShape, min(dorsal, ventral));
    }

    vec3 fins = mix(water, tint * 0.65, 0.60);
    water = mix(water, fins, coverage(finD * size));

    float body = ellipse(p, bodyR);
    float mask = coverage(body * size);
    vec2 q = p / bodyR;
    float rounding = sqrt(max(0.0, 1.0 - dot(q, q)));
    vec3 bodyColor;
    if (species == SPECIES_TETRA) {
        // Dark back, iridescent blue-cyan lateral stripe, red rear belly.
        bodyColor = tint * (.45 + .55 * rounding);
        float stripe = exp(-pow((p.y - .05) * 13.0, 2.0)) * smoothstep(-.70, -.35, p.x) * (1.0 - smoothstep(.45, .65, p.x));
        float red = smoothstep(.10, -.05, p.y) * smoothstep(.15, -.15, p.x);
        bodyColor = mix(bodyColor, vec3(.85, .10, .07) * (.5 + .5 * rounding), red * .9);
        bodyColor += vec3(.10, .62, 1.0) * stripe * (.75 + .25 * sin(iTime * 1.7 + seed * 3.0));
    } else if (species == SPECIES_ANGEL) {
        float bars = exp(-pow((p.x - .24) * 13.0, 2.0)) + exp(-pow((p.x + .06) * 11.0, 2.0)) * 1.1 +
                     exp(-pow((p.x + .34) * 13.0, 2.0)) * .8;
        bodyColor = tint * (.40 + .55 * rounding + .12 * p.y) * (1.0 - .72 * clamp(bars, 0.0, 1.0));
        bodyColor += vec3(.18, .16, .06) * exp(-pow((p.y - .20) * 6.0, 2.0)) * rounding;
    } else {
        float stripes = 0.85 + 0.15 * smoothstep(-0.2, 0.4, sin(p.x * 19.0 + seed));
        bodyColor = tint * (0.38 + 0.48 * rounding + 0.18 * p.y) * stripes;
        bodyColor += vec3(0.24, 0.29, 0.18) * exp(-pow((p.y - .16) * 15.0, 2.0)) * rounding;
        float gill = exp(-pow((p.x - .29) * 35.0, 2.0)) * (1.0 - smoothstep(.08, .30, abs(p.y)));
        bodyColor *= 1.0 - .20 * gill;
    }
    bodyColor = mix(bodyColor, vec3(0.015, 0.15, 0.19), depth);
    water = mix(water, bodyColor, mask);

    vec2 eyePos = vec2(bodyR.x * .63, bodyR.y * .24);
    float eye = coverage((length(p - eyePos) - .075) * size);
    water = mix(water, vec3(.012, .025, .025), eye * mask);
    float eyeLight = coverage((length(p - eyePos - vec2(.02, .03)) - .025) * size);
    water = mix(water, vec3(.65, .76, .60), eyeLight * mask * .65 * (1.0 - depth));
    return water;
}

// Slow wandering path. Returns (x, y) for time t.
vec2 wanderPath(float t, float seed) {
    float w = .042 + .022 * hash(seed + 1.0);
    float ph = seed * 2.7;
    float x = .5 + .40 * sin(w * t + ph) + .06 * sin(w * 2.3 * t + ph * 1.7);
    float y = .28 + .38 * hash(seed + 5.0) + .045 * sin(t * (.11 + .05 * hash(seed + 9.0)) + ph);
    return vec2(x, y);
}

// Foreground fish: wander, drift toward the flakes while feeding.
vec2 foregroundFish(float t, float seed) {
    vec2 pos = wanderPath(t, seed);
    float age = mod(t + 140.0, FEED_PERIOD);
    float feed = smoothstep(4.0, 11.0, age) * (1.0 - smoothstep(24.0, 36.0, age));
    vec2 feedSpot = vec2(.50 + .16 * sin(seed * 4.1 + t * .21), .70 + .06 * sin(t * .5 + seed));
    return mix(pos, feedSpot, feed * .75);
}

// Facing from the horizontal velocity; turns take roughly a second.
float facingFrom(float dx) { return dx / sqrt(dx * dx + 1.0e-6); }

vec2 schoolCenter(float t) {
    return vec2(.5 + .62 * sin(.037 * t + 1.0) + .05 * sin(.11 * t),
                .63 + .07 * sin(.061 * t) + .03 * sin(.13 * t + 2.0));
}

// ---------------------------------------------------------------- scenery
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

float surfaceHeight(float x) {
    return .925 + .009 * sin(x * 9.0 + iTime * .70) + .005 * sin(x * 23.0 - iTime * 1.10);
}

vec3 waterSurface(vec3 color, vec2 uv, float music) {
    float s = surfaceHeight(uv.x);
    // Underside of the surface: bright, rippled total-internal-reflection band.
    float above = smoothstep(s - aa, s + aa, uv.y);
    float ripple = .5 + .5 * sin(uv.x * 31.0 + sin(uv.x * 13.0 - iTime * .9) * 2.0 + iTime * .6);
    ripple *= .5 + .5 * sin(uv.x * 17.0 - iTime * .45 + 1.3);
    vec3 underside = mix(vec3(.05, .25, .27), vec3(.20, .50, .45), ripple) * (1.0 + .15 * music);
    underside *= .85 + .15 * smoothstep(1.0, s, uv.y);
    color = mix(color, underside, above);
    // Meniscus highlight and a soft glow just beneath it.
    float line = exp(-pow((uv.y - s) / (aa * 1.4), 2.0));
    color += vec3(.20, .38, .32) * line * (.55 + .45 * ripple);
    color += vec3(.04, .11, .10) * exp(-max(s - uv.y, 0.0) * 28.0) * (1.0 - above);
    return color;
}

vec3 rocks(vec3 color, vec2 uv) {
    for (int i = 0; i < 3; ++i) {
        float k = float(i);
        vec2 center = vec2(.17 + .33 * k + .02 * sin(k * 5.0), .040);
        vec2 radius = vec2(.12 + .015 * sin(k), .070 + .020 * hash(k + 3.0));
        vec2 p = uv - center;
        float d = ellipse(p, radius) + .004 * sin(p.x * 61.0 + k * 3.0) + .003 * sin(p.x * 23.0 - k);
        // Contact shadow on the sand around the rock's base.
        float shadow = (1.0 - smoothstep(0.0, .035, d)) * (1.0 - smoothstep(.07, .16, uv.y));
        color *= 1.0 - .40 * shadow;
        vec2 n = p / radius;
        float top = smoothstep(-.35, .95, n.y);
        float side = .5 + .5 * clamp(-n.x * .6 + .3, -1.0, 1.0);
        float grain = .82 + .18 * sin(uv.x * 67.0 + sin(uv.y * 91.0 + k)) * sin(uv.y * 53.0 - uv.x * 17.0);
        vec3 rockColor = mix(vec3(.030, .060, .055), vec3(.21, .25, .18), top * (.75 + .25 * side)) * grain;
        float caustic = pow(.5 + .5 * sin(uv.x * 43.0 + sin(uv.y * 37.0 + iTime * .55) + iTime * .35), 6.0);
        rockColor += vec3(.05, .08, .05) * caustic * top;
        float moss = smoothstep(.55, .85, n.y + .10 * sin(uv.x * 140.0 + k * 7.0));
        rockColor = mix(rockColor, vec3(.06, .19, .07) * (.8 + .2 * grain), moss * .8);
        color = mix(color, rockColor, coverage(d));
    }
    return color;
}

vec3 sandColorAt(vec2 uv, float brightness) {
    float caustics = pow(.5 + .5 * sin(uv.x * 39.0 + sin(uv.y * 32.0 + iTime * .6) + iTime * .4), 5.0);
    float grain = .92 + .08 * sin(uv.x * 211.0 + sin(uv.y * 173.0) * 3.0);
    return (vec3(.15, .17, .105) * brightness + vec3(.04, .07, .035) * caustics) * grain;
}

// ---------------------------------------------------------------- main
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.y;
    float aspect = iResolution.x / iResolution.y;
    uv.x -= (aspect - 1.0) * .5;
    aa = .65 / iResolution.y;
    float live = step(.5, iAudioAvailable) * (1.0 - step(.5, iAudioSilence));
    float music = live * clamp(iAudioLoudness, 0.0, 1.0);

    // Water body with depth falloff and window glow.
    vec3 color = mix(vec3(.008, .045, .060), vec3(.018, .22, .26), pow(uv.y, .8));
    float windowGlow = exp(-2.7 * pow((uv.x - .35) / .65, 2.0)) * pow(uv.y, 2.0);
    color += vec3(.075, .20, .16) * windowGlow;

    // Light shafts hanging from the surface, flickering with its ripples.
    for (int i = 0; i < 5; ++i) {
        float k = float(i);
        float ray = uv.x - .06 - k * .21 + (1.0 - uv.y) * .25 + .025 * sin(iTime * .15 + k);
        float flicker = .70 + .30 * sin(iTime * (.37 + .05 * k) + k * 2.3);
        color += vec3(.06, .15, .12) * exp(-pow(ray / (.030 + .012 * hash(k)), 2.0)) * (.10 + .90 * uv.y) * .26 * flicker;
    }

    // Sand bed.
    float sandLine = .11 + .015 * sin(uv.x * 7.0) + .008 * sin(uv.x * 19.0);
    color = mix(color, sandColorAt(uv, .6 + .3 * uv.y / .12), coverage(uv.y - sandLine));

    color = plants(color, uv, 0.0, music);
    color = leafyPlants(color, uv);

    // Distant background cichlid, hazy.
    {
        vec2 pos = wanderPath(iTime * .8, 61.0);
        pos.y = .50 + .10 * sin(iTime * .05);
        float dx = wanderPath(iTime * .8, 61.0).x - wanderPath(iTime * .8 - .4, 61.0).x;
        color = fish(color, uv, pos, .036, facingFrom(dx), 13.0, .62, SPECIES_CICHLID);
    }

    // Neon tetra school: members follow the leader with individual lag, so a
    // turn ripples through the group. They part around the visitor.
    float vX = visitorX();
    float vY = .57 + .04 * sin(visitorAge() * .22);
    for (int i = 0; i < 14; ++i) {
        float k = float(i);
        float lag = 2.2 * hash(k + 50.0);
        float t = iTime - lag;
        vec2 offset = (vec2(hash(k + 11.0), hash(k + 23.0)) - .5) * vec2(.24, .12);
        vec2 pos = schoolCenter(t) + offset + .010 * vec2(sin(iTime * .53 + k), sin(iTime * .71 + k * 1.9));
        float dx = schoolCenter(t).x - schoolCenter(t - .4).x;
        float parting = exp(-pow((pos.x - vX) * 6.0, 2.0)) * .08;
        pos.y += parting * (pos.y > vY ? 1.0 : -1.0);
        color = fish(color, uv, pos, .026 + .003 * hash(k), facingFrom(dx), k + 21.0, .30, SPECIES_TETRA);
    }

    // Visitor: a large cichlid crossing roughly every 73 seconds.
    color = fish(color, uv, vec2(vX, vY), .098, 1.0, 5.0, .20, SPECIES_CICHLID);

    // Foreground fish: two cichlids and an angelfish wandering and turning.
    for (int i = 0; i < 3; ++i) {
        float k = float(i);
        float seed = k * 3.3 + 4.0;
        vec2 pos = foregroundFish(iTime, seed);
        float dx = pos.x - foregroundFish(iTime - .4, seed).x;
        int species = i == 2 ? SPECIES_ANGEL : SPECIES_CICHLID;
        float size = species == SPECIES_ANGEL ? .080 : .064 + .010 * hash(k + 7.0);
        color = fish(color, uv, pos, size, facingFrom(dx), k == 1.0 ? 4.0 : 7.0, .06, species);
    }

    // Food flakes during the feeding event.
    float fAge = feedAge();
    if (fAge < 40.0) {
        for (int i = 0; i < 12; ++i) {
            float k = float(i);
            float age = fAge - 1.6 * k * hash(k + 70.0);
            float y = surfaceHeight(.5) - .005 - age * .020;
            float x = .36 + .28 * hash(k + 80.0) + .012 * sin(age * 1.3 + k);
            float vis = smoothstep(0.0, .6, age) * (1.0 - smoothstep(22.0, 28.0, age));
            vec2 d = uv - vec2(x, y);
            color += vec3(.55, .32, .10) * exp(-dot(d, d) * 26000.0) * vis * .9;
        }
    }

    color = rocks(color, uv);

    // Front sand lip half-burying the rocks.
    float frontSand = .030 + .007 * sin(uv.x * 11.0 + 1.0) + .004 * sin(uv.x * 29.0);
    color = mix(color, sandColorAt(uv, 1.05), coverage(uv.y - frontSand));

    color = plants(color, uv, 1.0, music);

    // Bubbles rising from behind the right rock.
    for (int i = 0; i < 9; ++i) {
        float k = float(i);
        float age = mod(iTime + k * .33, 37.0);
        float visibility = smoothstep(0.0, .4, age) * (1.0 - smoothstep(6.0, 6.6, age));
        vec2 center = vec2(.80 + .018 * sin(age * 2.0 + k), .12 + age * .125);
        float radius = .006 + .003 * hash(k + 2.0);
        float d = length(uv - center);
        float bubble = exp(-pow((d - radius) / (aa * .8), 2.0));
        color += vec3(.11, .28, .27) * bubble * visibility * .55;
    }

    // Drifting motes.
    for (int i = 0; i < 14; ++i) {
        float k = float(i);
        vec2 mote = vec2(hash(k + 30.0) + .016 * sin(iTime * .12 + k), fract(hash(k + 60.0) + iTime * .003));
        float fade = smoothstep(.0, .10, mote.y) * (1.0 - smoothstep(.80, .90, mote.y));
        color += vec3(.06, .12, .10) * exp(-dot(uv - mote, uv - mote) * 65000.0) * fade;
    }

    color = waterSurface(color, uv, music);

    color *= .85 + .15 * exp(-2.0 * dot(uv - .5, uv - .5));
    fragColor = vec4(1.0 - exp(-color * 1.9), 1.0);
}
