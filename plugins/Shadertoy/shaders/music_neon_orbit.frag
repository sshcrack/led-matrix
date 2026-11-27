/* led-matrix-shader
{
  "family": "orbital",
  "tags": ["music", "neon", "space", "planets", "audio-reactive", "beat-driven", "showcase", "style:edm", "style:party"],
  "intensity": 0.78,
  "motion": 0.85,
  "music_affinity": 1.0,
  "performance_cost": 0.34,
  "automatic_eligible": true,
  "audio_reactive": true
}
*/

// A neon star system seen from slightly above. A pulsing star sits at the
// centre; five glossy planets swing round it on tilted orbits and pass behind
// and in front of it, each dragging a light trail. Each planet is driven by a
// band of the spectrum (it speeds up and swells with its band), the star's
// corona fires flares on kicks, and a drop sends a shock front through the
// system.

#define TAU 6.28318530718

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

const float TILT = 0.42;   // orbit plane squash (view from above)

// Planet i: x/y on screen, z>0 behind the star.
vec3 planetPos(float radius, float angle) {
    return vec3(cos(angle) * radius, sin(angle) * radius * TILT, sin(angle));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float px = 1.0 / iResolution.y;
    float live = step(0.5, iAudioAvailable) * (1.0 - step(0.5, iAudioSilence));
    float bass = mix(0.22 + 0.06 * sin(iTime * 1.1), iAudioBass, live);
    float kick = mix(0.0, iAudioKick, live);
    float beat = mix(0.20 + 0.10 * sin(iTime * 2.0), iAudioBeatStrength, live);
    float treble = mix(0.12, iAudioTreble, live);
    float drop = live * iAudioDrop;

    // Whole system rotated a little so orbits read as tilted ellipses.
    float sys = -0.22;
    mat2 R = mat2(cos(sys), -sin(sys), sin(sys), cos(sys));
    vec2 p = R * uv;

    // Deep space: dark blue with a magenta nebula band and stars.
    vec3 col = vec3(0.010, 0.012, 0.035);
    float band = exp(-pow((uv.y + 0.35 * uv.x) * 3.2, 2.0));
    col += vec3(0.10, 0.02, 0.10) * band * (0.6 + 0.4 * sin(uv.x * 4.0 + iTime * 0.05));
    vec2 g = uv * 16.0 + vec2(iTime * 0.04, 0.0);
    float seed = hash21(floor(g));
    vec2 jitter = vec2(hash21(floor(g) + 7.0), hash21(floor(g) + 13.0)) - 0.5;
    col += vec3(0.75, 0.8, 1.0) * step(0.965, seed) * smoothstep(0.22, 0.04, length(fract(g) - 0.5 - jitter * 0.5))
         * (0.35 + 0.35 * sin(iTime * (1.0 + seed * 2.0) + seed * 30.0)) * (0.7 + treble);

    // Planet setup.
    vec3 pcol[5] = vec3[5](vec3(0.20, 0.95, 1.00), vec3(1.00, 0.25, 0.75), vec3(0.65, 1.00, 0.25),
                           vec3(1.00, 0.60, 0.15), vec3(0.55, 0.40, 1.00));
    float radii[5] = float[5](0.16, 0.24, 0.32, 0.40, 0.47);
    float sizes[5] = float[5](0.022, 0.034, 0.030, 0.044, 0.028);

    vec3 behind = vec3(0.0);   // planets/trails on the far side of the star
    vec3 front = vec3(0.0);
    float frontMask = 0.0;
    vec3 frontCol = vec3(0.0);
    float behindMask = 0.0;
    vec3 behindCol = vec3(0.0);

    for (int i = 0; i < 5; ++i) {
        float fi = float(i);
        float energy = live * sampleAudioSpectrum(iChannel0, 0.03 + fi * 0.1);
        float speed = 0.55 / pow(radii[i] / 0.17, 1.5);          // Kepler-ish
        float ang = iTime * speed * (1.0 + 0.6 * energy) + fi * 1.9;
        float rad = radii[i] * (1.0 + 0.04 * bass);
        vec3 pp = planetPos(rad, ang);

        // Orbit path: faint dotted ellipse.
        vec2 e = p / vec2(rad, rad * TILT);
        float ed = abs(length(e) - 1.0) * rad * TILT;
        float pathAng = atan(e.y, e.x);
        float dots = smoothstep(0.55, 0.9, 0.5 + 0.5 * cos(pathAng * (18.0 + fi * 6.0)));
        vec3 path = pcol[i] * smoothstep(1.4 * px, 0.0, ed) * (0.03 + 0.07 * dots);

        // Trail: the ellipse behind the planet, fading with angular lag.
        float lag = mod(ang - pathAng, TAU);
        float trailLen = 1.1 + 0.8 * energy;
        float trail = smoothstep(2.2 * px, 0.0, ed) * exp(-lag / trailLen * 3.0) * step(lag, trailLen * 1.5);
        vec3 trailCol = pcol[i] * trail * (0.45 + 0.6 * energy);

        // Planet sphere: lit from the star (centre).
        float size = sizes[i] * (1.0 + 0.25 * energy) * (1.0 - 0.18 * pp.z);
        vec2 d = p - pp.xy;
        float cover = smoothstep(size + px, size - px, length(d));
        vec2 m = d / size;
        m /= max(1.0, length(m));
        vec3 nrm = vec3(m, sqrt(max(0.0, 1.0 - dot(m, m))));
        vec3 toStar = normalize(vec3(-pp.xy, 0.15));
        float lit = max(dot(nrm, toStar), 0.0);
        vec3 planet = pcol[i] * (0.10 + 0.95 * lit) + vec3(1.0) * pow(lit, 14.0) * 0.6;
        float halo = exp(-max(length(d) - size, 0.0) / (0.025 + 0.02 * energy)) * (1.0 - cover);

        // Ring around the big one.
        if (i == 3) {
            vec2 rq = d / vec2(size * 2.0, size * 0.55);
            float ring = smoothstep(0.14, 0.0, abs(length(rq) - 1.0)) * (1.0 - step(0.0, rq.y) * cover);
            planet = mix(planet, vec3(1.0, 0.85, 0.55), ring * 0.8);
            cover = max(cover, ring * 0.9);
        }

        if (pp.z > 0.0) {
            behindCol = mix(behindCol, planet, cover);
            behindMask = max(behindMask, cover);
            behind += pcol[i] * halo * 0.25;
        } else {
            frontCol = mix(frontCol, planet, cover);
            frontMask = max(frontMask, cover);
            front += pcol[i] * halo * 0.25;
        }
        col += path + trailCol;
    }

    // Draw the far side first, then the star, then the near side.
    col = mix(col, behindCol, behindMask);
    col += behind;

    // Star with corona and kick flares (spiky rays).
    float r = length(p);
    float a = atan(p.y, p.x);
    float starR = 0.065 + 0.012 * bass + 0.010 * kick;
    float core = smoothstep(starR + px, starR - px, r);
    vec3 starCol = mix(vec3(1.0, 0.55, 0.95), vec3(1.0, 0.95, 0.85), smoothstep(starR, 0.0, r));
    float rays = pow(0.5 + 0.5 * cos(a * 6.0 + iTime * 0.6), 18.0) + pow(0.5 + 0.5 * cos(a * 4.0 - iTime * 0.4 + 1.0), 24.0);
    float corona = exp(-(r - starR) * 16.0) * (0.35 + 0.5 * beat) + rays * exp(-(r - starR) * (9.0 - 5.0 * kick)) * (0.15 + 0.9 * kick);
    col += vec3(1.0, 0.35, 0.85) * corona * step(starR, r);
    col = mix(col, starCol, core);

    col = mix(col, frontCol, frontMask);
    col += front;

    // Drop: a shock front expanding through the orbit plane.
    float shockR = fract(iTime * 0.6) * 0.7;
    vec2 sq = p / vec2(1.0, TILT);
    col += vec3(0.6, 0.9, 1.0) * exp(-pow((length(sq) - shockR) / 0.012, 2.0)) * drop * (1.0 - shockR / 0.7);

    col = 1.0 - exp(-col * 1.5);
    fragColor = vec4(col, 1.0);
}
