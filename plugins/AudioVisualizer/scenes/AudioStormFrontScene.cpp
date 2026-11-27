#include <array>

#include "GenreVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

namespace {
// Value noise tiled horizontally with period `period` cells.
float vnoise(float x, float y, int period, uint32_t seed)
{
    const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
    const float fx = x - xi, fy = y - yi;
    auto h = [&](int ix, int iy) {
        const int wx = ((ix % period) + period) % period;
        return hash(static_cast<uint32_t>(wx) * 73856093u ^ static_cast<uint32_t>(iy) * 19349663u ^ seed);
    };
    const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    const float a = h(xi, yi) + (h(xi + 1, yi) - h(xi, yi)) * sx;
    const float b = h(xi, yi + 1) + (h(xi + 1, yi + 1) - h(xi, yi + 1)) * sx;
    return a + (b - a) * sy;
}
}  // namespace

void AudioStormFrontScene::register_properties()
{
    add_property(sensitivity_);
    add_property(rainAmount_);
    add_property(boltRate_);
}

void AudioStormFrontScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    const size_t N = static_cast<size_t>(W * H);
    horizon_ = static_cast<int>(std::lround(H * 0.55f));

    // --- cloud decks ----------------------------------------------------------------------------------------
    cloudA_.assign(N, 0.0f);
    cloudB_.assign(N, 0.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float fx = static_cast<float>(x) / W, fy = static_cast<float>(y) / H;
            auto fbm = [&](uint32_t seed) {
                float v = 0, amp = 0.55f, tot = 0;
                int period = 4;
                for (int o = 0; o < 4; ++o) {
                    v += amp * vnoise(fx * period, fy * 5.0f * (o + 1) * 0.5f, period, seed + o);
                    tot += amp;
                    amp *= 0.5f;
                    period *= 2;
                }
                return v / tot;
            };
            cloudA_[static_cast<size_t>(y * W + x)] = fbm(11u);
            cloudB_[static_cast<size_t>(y * W + x)] = fbm(97u);
        }

    // --- sea: per-pixel wave phase with perspective ------------------------------------------------------------
    seaPhase_.assign(N, 0.0f);
    seaFoamSeed_.assign(N, 1.0f);
    for (int y = horizon_; y < H; ++y) {
        const float z = static_cast<float>(y - horizon_ + 1) / static_cast<float>(H - horizon_);
        const float depth = 1.0f / (z + 0.06f);
        for (int x = 0; x < W; ++x) {
            const float fx = static_cast<float>(x);
            const float phase = depth * 1.5f + 0.30f * std::sin(fx * 0.08f / (z + 0.15f) + depth * 2.1f) +
                                0.10f * std::sin(fx * 0.23f / (z + 0.25f) - depth * 1.3f);
            seaPhase_[static_cast<size_t>(y * W + x)] = phase;
            seaFoamSeed_[static_cast<size_t>(y * W + x)] = vnoise(fx * 0.35f / (z + 0.1f), depth * 3.0f, 1 << 20, 404u);
        }
    }

    // --- headland with a fishing village, a pier ----------------------------------------------------------------
    landBase_.assign(N, RGB{});
    landLit_.assign(N, -1.0f);
    windowBand_.assign(N, static_cast<int8_t>(-1));
    cliffFoot_.clear();
    const float sx = W / 128.0f, sy = H / 128.0f;  // design space is 128 x 128
    const float footBase = H * 0.80f;
    auto footY = [&](int x) { return footBase + 1.5f * std::sin(x * 0.31f) + std::sin(x * 0.77f); };
    auto edgeX = [&](float y) {
        return W * 0.50f + (footBase - y) * 0.22f * sx / sy + 1.6f * (vnoise(y * 0.35f, 0.5f, 1 << 20, 77u) - 0.5f) * 2.0f;
    };
    auto topY = [&](int x) {
        const float plateau = H * 0.39f + 1.2f * std::sin(x * 0.11f) + 0.8f * std::sin(x * 0.37f);
        // Round the shoulder where the plateau meets the cliff face.
        const float shoulder = std::max(0.0f, (W * 0.62f - x) / sx) * 0.45f * sy;
        return plateau + shoulder;
    };
    auto setLand = [&](int x, int y, const RGB& c, float lit) {
        if (x < 0 || y < 0 || x >= W || y >= H)
            return;
        const size_t i = static_cast<size_t>(y * W + x);
        landBase_[i] = c;
        landLit_[i] = lit;
        windowBand_[i] = -1;
    };
    for (int x = 0; x < W; ++x) {
        const int fy = static_cast<int>(footY(x));
        const int ty = static_cast<int>(topY(x));
        bool any = false;
        for (int y = ty; y < fy; ++y) {
            if (x < edgeX(static_cast<float>(y)))
                continue;
            any = true;
            const float n = hash(static_cast<uint32_t>(x * 911 + y * 131));
            const float crack = vnoise(x * 0.45f, y * 0.12f, 1 << 20, 515u);                     // vertical joints
            const float ledge = vnoise(x * 0.10f, y * 0.50f, 1 << 20, 733u);                     // horizontal strata
            const float face = static_cast<float>(y - ty) / std::max(1, fy - ty);                // 0 top .. 1 foot
            const float rim = std::max(0.0f, 1.0f - (x - edgeX(static_cast<float>(y))) / 3.0f);  // sky-lit left edge
            float shade = (0.55f + 0.30f * n) * (0.55f + 0.6f * crack) * (0.8f + 0.4f * ledge) * (1.0f - 0.30f * face);
            RGB rock = RGB{52, 50, 56} * shade + RGB{64, 72, 92} * rim;
            float lit = 0.55f + 0.35f * rim;
            if (y - ty < 2 || (ledge > 0.78f && crack > 0.45f && face < 0.7f)) {  // grass on top and on ledges
                rock = RGB{26, 40, 28} * (0.8f + 0.2f * n);
                lit = 0.45f;
            }
            setLand(x, y, rock, lit);
        }
        if (any)
            cliffFoot_.push_back({x, fy});
    }

    // Cottages along the plateau, a church in the middle; whitewashed walls, dark roofs.
    struct House {
        float x;  // left edge, design px
        int w, h;
        RGB wall;
    };
    const House houses[] = {{80, 9, 7, {52, 50, 54}},
                            {90, 8, 6, {58, 48, 42}},
                            {111, 10, 8, {46, 52, 58}},
                            {122, 7, 6, {56, 54, 50}},
                            {72.5f, 6, 5, {44, 44, 50}}};
    int windowIndex = 0;
    auto drawHouse = [&](const House& hs) {
        const int x0 = static_cast<int>(hs.x * sx), w = std::max(4, static_cast<int>(hs.w * sx));
        const int h = std::max(3, static_cast<int>(hs.h * sy));
        int base = H;
        for (int x = x0; x < x0 + w; ++x) base = std::min(base, static_cast<int>(topY(x)) + 1);
        const int roofH = std::max(2, w / 2);
        for (int y = base - h; y < base; ++y)
            for (int x = x0; x < x0 + w; ++x) setLand(x, y, hs.wall * (x == x0 ? 0.75f : 1.0f), 1.0f);
        for (int k = 0; k < roofH; ++k)
            for (int x = x0 - 1 + k; x <= x0 + w - k; ++x)
                setLand(x, base - h - 1 - k, k == roofH - 1 ? RGB{70, 74, 92} : RGB{46, 38, 46}, 0.35f);
        if (w >= 8)  // chimney
            for (int y = base - h - roofH; y < base - h - roofH / 2; ++y) setLand(x0 + w - 3, y, RGB{30, 24, 26}, 0.35f);
        // Two windows per storey, 2x2, a few dark.
        for (int wx = x0 + 1; wx + 1 < x0 + w - 1; wx += 4) {
            const int wy = base - h + 2;
            const bool on = hash(static_cast<uint32_t>(windowIndex * 2654435761u + 7u)) > 0.18f;
            const auto band = static_cast<int8_t>((windowIndex * 5) % 12);
            ++windowIndex;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    setLand(wx + dx, wy + dy, RGB{14, 12, 16}, 0.2f);
                    if (on && wx + dx < W && wy + dy >= 0)
                        windowBand_[static_cast<size_t>((wy + dy) * W + wx + dx)] = band;
                }
        }
    };
    for (const auto& hs : houses) drawHouse(hs);
    {  // church: tower with a spire and an arched lit window
        const int x0 = static_cast<int>(100 * sx), w = std::max(4, static_cast<int>(6 * sx));
        int base = H;
        for (int x = x0 - 6; x < x0 + w; ++x) base = std::min(base, static_cast<int>(topY(x)) + 1);
        const int towerH = static_cast<int>(15 * sy), spireH = static_cast<int>(11 * sy);
        for (int y = base - towerH; y < base; ++y)
            for (int x = x0; x < x0 + w; ++x) setLand(x, y, RGB{50, 50, 56} * (x == x0 ? 0.75f : 1.0f), 1.0f);
        for (int k = 0; k < spireH; ++k) {
            const float half = (w * 0.5f + 0.5f) * (1.0f - static_cast<float>(k) / spireH);
            const float c = x0 + w * 0.5f - 0.5f;
            for (int x = static_cast<int>(std::floor(c - half + 0.5f)); x <= static_cast<int>(std::ceil(c + half - 0.5f)); ++x)
                setLand(x, base - towerH - 1 - k, x <= static_cast<int>(c) ? RGB{74, 80, 100} : RGB{40, 38, 48}, 0.35f);
        }
        // Nave to the left of the tower.
        for (int y = base - static_cast<int>(7 * sy); y < base; ++y)
            for (int x = x0 - static_cast<int>(6 * sx); x < x0; ++x) setLand(x, y, RGB{46, 46, 52}, 1.0f);
        for (int k = 0; k < 3; ++k)
            for (int x = x0 - static_cast<int>(6 * sx) - 1 + k; x < x0; ++x)
                setLand(x, base - static_cast<int>(7 * sy) - 1 - k, RGB{46, 38, 46}, 0.35f);
        const int wx = x0 + w / 2 - 1, wy = base - towerH + 3;
        for (int dy = 0; dy < 3; ++dy)
            for (int dx = 0; dx < 2; ++dx) {
                setLand(wx + dx, wy + dy, RGB{14, 12, 16}, 0.2f);
                windowBand_[static_cast<size_t>((wy + dy) * W + wx + dx)] = static_cast<int8_t>(2);
            }
        for (int wxn = x0 - static_cast<int>(5 * sx); wxn < x0 - 1; wxn += 3) {
            const int wyn = base - static_cast<int>(5 * sy);
            for (int dy = 0; dy < 2; ++dy) {
                setLand(wxn, wyn + dy, RGB{14, 12, 16}, 0.2f);
                windowBand_[static_cast<size_t>((wyn + dy) * W + wxn)] = static_cast<int8_t>(8);
            }
        }
    }

    // Pier from the cliff foot out into the harbour, with a lamp at its end.
    const int deckY = static_cast<int>(footBase - 3 * sy);
    const int pierX0 = static_cast<int>(W * 0.34f), pierX1 = static_cast<int>(edgeX(static_cast<float>(deckY))) + 1;
    for (int x = pierX0; x < pierX1; ++x) {
        setLand(x, deckY, RGB{78, 62, 50}, 0.6f);
        setLand(x, deckY + 1, RGB{22, 18, 16}, 0.4f);
        if ((x - pierX0) % 5 == 0)
            for (int y = deckY + 2; y < deckY + 2 + static_cast<int>(5 * sy); ++y) setLand(x, y, RGB{18, 15, 14}, 0.3f);
    }
    pierLampX_ = pierX0 + 1.5f;
    pierLampY_ = deckY - 7.0f * sy;
    for (int y = static_cast<int>(pierLampY_) + 1; y < deckY; ++y) setLand(static_cast<int>(pierLampX_), y, RGB{20, 20, 22}, 0.3f);

    boatX_ = W * 0.20f;
    boatY_ = H * 0.88f;

    rain_.clear();
    for (int i = 0; i < 260; ++i) rain_.push_back({rnd() * W, rnd() * H, 0.7f + rnd() * 0.6f});
    bolts_.clear();
    cloudFlashes_.clear();
    spray_.clear();
}

void AudioStormFrontScene::strike(float x)
{
    const int W = matrix_width, H = matrix_height;
    Bolt b;
    float cx = x, cy = 0.0f;
    const float ground = static_cast<float>(horizon_) + 1.0f;
    b.pts.push_back({cx, cy});
    while (cy < ground) {
        cy += 3.0f + rnd() * 4.0f;
        cx += (rnd() - 0.5f) * 9.0f;
        cx = std::clamp(cx, 2.0f, W - 3.0f);
        b.pts.push_back({cx, std::min(cy, ground)});
        // Side branches leave from the middle of the bolt.
        if (rnd() < 0.25f && cy > H * 0.12f && cy < ground - 8.0f) {
            float bx = cx, by = cy;
            const float dir = rnd() < 0.5f ? -1.0f : 1.0f;
            b.branch.push_back({bx, by});
            for (int k = 0; k < 4; ++k) {
                bx += dir * (2.0f + rnd() * 4.0f);
                by += 2.0f + rnd() * 3.0f;
                b.branch.push_back({bx, by});
            }
            b.branch.push_back({-1.0f, -1.0f});  // separator
        }
    }
    b.x = cx;
    b.life = 1.0f;
    bolts_.push_back(std::move(b));
    if (bolts_.size() > 4)
        bolts_.erase(bolts_.begin());
    sinceStrike_ = 0.0f;
}

bool AudioStormFrontScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (cloudA_.size() != static_cast<size_t>(W * H))
        init_world();
    spectrum_.update(audio, dt, 0.05f, 0.25f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    const float snare = feature(audio, AudioProtocol::Feature::Snare);
    const float bass = spectrum_.band(0.0f, 0.2f);
    const float treble = spectrum_.band(0.55f, 1.0f);
    const float rate = boltRate_->get();
    sinceStrike_ += dt;
    // Bolts on hits (stronger hits and louder passages strike more often, with a short refractory time);
    // a quiet passage still gets one now and then.
    const float hit = std::max(kick, snare);
    if (events_.onsetHit && sinceStrike_ > 1.1f / rate && rnd() < (0.15f + 0.35f * hit + 0.30f * spectrum_.level) * rate) {
        strike(W * (0.05f + 0.90f * rnd()));
        flash_.hit(0.6f + 0.3f * rnd());
    }
    else if (events_.beatHit && sinceStrike_ > 5.0f / rate) {
        strike(W * (0.05f + 0.55f * rnd()));
        flash_.hit(0.6f);
    }
    // Sheet lightning inside the clouds on beats.
    if (events_.beatHit && rnd() < 0.55f) {
        cloudFlashes_.push_back({rnd() * W, H * (0.08f + 0.30f * rnd()), 14.0f + rnd() * 14.0f, 0.5f + 0.5f * rnd()});
        if (cloudFlashes_.size() > 6)
            cloudFlashes_.erase(cloudFlashes_.begin());
    }
    if (events_.onsetHit && kick > 0.35f) {
        kick_.hit(kick);
        swell_.hit(0.5f + 0.5f * kick);
        // Spray where the waves break on the cliff.
        const int n = static_cast<int>(4 + 8 * kick);
        for (int i = 0; i < n && !cliffFoot_.empty(); ++i) {
            const auto& f = cliffFoot_[static_cast<size_t>(rnd() * (cliffFoot_.size() - 1))];
            spray_.push_back(
                {f.first + 0.5f, static_cast<float>(f.second), (rnd() - 0.6f) * 14.0f, -(18.0f + 30.0f * rnd()) * (0.6f + kick), 1.0f});
        }
    }
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
        for (int i = 0; i < 3; ++i) strike(W * (0.05f + 0.9f * rnd()));
        flash_.hit();
        swell_.hit();
    }
    if (events_.sectionHit)
        palette_.next();
    flash_.decay(dt, 0.18f);
    kick_.decay(dt, 0.2f);
    swell_.decay(dt, 0.9f);
    drop_.decay(dt, 2.0f);
    palette_.update(dt, 2.0f);
    for (auto& b : bolts_) b.life -= dt / 0.35f;
    std::erase_if(bolts_, [](const Bolt& b) { return b.life <= 0.0f; });
    for (auto& f : cloudFlashes_) f.life -= dt / 0.30f;
    std::erase_if(cloudFlashes_, [](const CloudFlash& f) { return f.life <= 0.0f; });
    wind_ += dt * (6.0f + 22.0f * spectrum_.level);
    waves_ += dt * (0.35f + 0.45f * spectrum_.level + 0.5f * swell_.v);

    const int hz = horizon_;
    const RGB tint = hsv(palette_.value, 0.45f, 1.0f);
    float lightning = std::min(1.0f, flash_.v);
    for (const auto& b : bolts_) lightning = std::max(lightning, 0.55f * b.life);
    const float villageX = W * 0.80f, villageY = H * 0.36f;

    // --- sky and two cloud decks, warm village glow from below --------------------------------------------------------
    for (int y = 0; y < hz; ++y) {
        const float t = static_cast<float>(y) / hz;
        const RGB sky = lerp(RGB{8, 10, 20}, RGB{40, 48, 66}, t * t) + tint * 0.04f;
        const int ox = static_cast<int>(wind_ * 0.4f), ox2 = static_cast<int>(wind_ * 0.9f);
        for (int x = 0; x < W; ++x) {
            const float a = cloudA_[static_cast<size_t>(y * W + (x + ox) % W)];
            const float b2 = cloudB_[static_cast<size_t>(y * W + (x + ox2) % W)];
            const float dens = std::clamp(((a * 0.6f + b2 * 0.5f) - 0.28f) * 2.4f * (0.35f + (1.0f - t)), 0.0f, 1.0f);
            const float glowD = std::max(0.0f, 1.0f - len((x - villageX) / 44.0f, (y - villageY) / 30.0f));
            const RGB cloud = RGB{40, 43, 58} * (0.55f + 0.9f * a * a) + tint * (0.06f + 0.10f * lightning) +
                              RGB{220, 225, 255} * (lightning * 0.30f * dens);
            buf_.at(x, y) = lerp(sky, cloud, dens) + RGB{70, 44, 20} * (glowD * glowD * (0.35f + 0.25f * dens));
        }
    }
    for (const auto& f : cloudFlashes_) glow(buf_, f.x, f.y, f.r, RGB{150, 155, 215} * (0.35f * f.life));

    // --- bolts --------------------------------------------------------------------------------------------------------
    for (const auto& b : bolts_) {
        const float a = std::clamp(b.life, 0.0f, 1.0f);
        const RGB core = RGB{235, 240, 255} * (1.4f * a);
        for (size_t i = 1; i < b.pts.size(); ++i) {
            line(buf_, b.pts[i - 1].first, b.pts[i - 1].second, b.pts[i].first, b.pts[i].second, core);
            glow(buf_, b.pts[i].first, b.pts[i].second, 6.0f, RGB{110, 120, 230} * (0.35f * a));
        }
        for (size_t i = 1; i < b.branch.size(); ++i) {
            if (b.branch[i].first < 0.0f || b.branch[i - 1].first < 0.0f)
                continue;
            line(buf_, b.branch[i - 1].first, b.branch[i - 1].second, b.branch[i].first, b.branch[i].second, core * 0.5f);
        }
    }

    // --- sea: rolling rows of waves, whitecaps with the energy, lightning and lamp reflections --------------------------
    const float foamWidth = 0.08f + 0.10f * swell_.v + 0.05f * bass;
    const float foamAmount = 0.30f + 0.40f * spectrum_.level + 0.30f * swell_.v;
    const RGB lampC{255, 190, 110};
    for (int y = hz; y < H; ++y) {
        const float z = static_cast<float>(y - hz + 1) / static_cast<float>(H - hz);
        const RGB base = lerp(RGB{40, 52, 68}, RGB{8, 20, 28}, std::sqrt(z)) + tint * 0.03f;
        const RGB lit = RGB{60, 70, 95} * (lightning * (1.0f - 0.55f * z));
        const float lampSpread = 0.8f + 2.5f * z;
        for (int x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y * W + x);
            const float c = fract(seaPhase_[i] - waves_);
            // Wave face: dark trough rising to a lit crest.
            const float shade = 0.80f + 0.45f * smoothstep(0.35f, 0.95f, c);
            RGB p = base * shade + lit;
            if (c > 1.0f - foamWidth * (1.0f - 0.75f * z) && seaFoamSeed_[i] < foamAmount)
                p = p + RGB{120, 135, 150} * (0.35f + 0.65f * z) * (0.6f + 0.4f * lightning);
            if (y > pierLampY_ && std::fabs(x + 0.5f - pierLampX_) < lampSpread && c > 0.55f && c < 0.9f)
                p = p + lampC * (0.22f * (1.0f - std::fabs(x + 0.5f - pierLampX_) / lampSpread) * (1.0f - 0.5f * z));
            for (const auto& b : bolts_) {
                const float spread = 1.0f + 3.0f * z;
                const float dx = std::fabs(x + 0.5f - b.x);
                if (dx < spread && c > 0.4f)
                    p = p + RGB{200, 210, 255} * (0.55f * b.life * (1.0f - dx / spread) * (1.0f - 0.5f * z));
            }
            buf_.at(x, y) = p;
        }
    }

    // --- headland, village, pier --------------------------------------------------------------------------------------
    std::array<float, 12> bandLevel{};
    for (int b = 0; b < 12; ++b) bandLevel[static_cast<size_t>(b)] = spectrum_.band(b / 12.0f, (b + 1) / 12.0f);
    const RGB landFlash = RGB{90, 95, 125} * lightning;
    for (size_t i = 0; i < landLit_.size(); ++i) {
        if (landLit_[i] < 0.0f)
            continue;
        const int band = windowBand_[i];
        if (band >= 0) {
            const float lv = bandLevel[static_cast<size_t>(band)];
            buf_.px[i] = lampC * (0.55f + 0.55f * lv + 0.10f * kick_.v);
        }
        else
            buf_.px[i] = landBase_[i] + landFlash * landLit_[i];
    }
    // Pier lamp.
    glow(buf_, pierLampX_ + 0.5f, pierLampY_ + 0.5f, 5.0f, lampC * 0.35f);
    buf_.set(static_cast<int>(pierLampX_), static_cast<int>(pierLampY_), RGB{255, 220, 160});

    // Surf where the sea meets the cliff.
    for (const auto& [fx, fy] : cliffFoot_) {
        const float n = hash(static_cast<uint32_t>(fx * 7919 + static_cast<int>(waves_ * 6.0f) * 31));
        const float s = 0.35f + 0.45f * swell_.v + 0.25f * n;
        buf_.add(fx, fy, RGB{150, 165, 180} * s);
        buf_.add(fx, fy - 1, RGB{150, 165, 180} * (s * 0.5f * n));
    }
    for (auto& sp : spray_) {
        sp.vy += 60.0f * dt;
        sp.x += sp.vx * dt;
        sp.y += sp.vy * dt;
        sp.life -= dt / 1.1f;
    }
    std::erase_if(spray_, [](const Spray& sp) { return sp.life <= 0.0f; });
    for (const auto& sp : spray_) buf_.splat(sp.x, sp.y, RGB{170, 185, 205} * (0.8f * sp.life));

    // --- fishing boat moored off the pier, rocking on the swell -------------------------------------------------------
    {
        const float tilt = 0.10f * std::sin(time_ * 1.3f) + 0.12f * swell_.v * std::sin(time_ * 2.6f);
        const float bob = 1.2f * std::sin(time_ * 1.3f + 0.8f) + 1.0f * swell_.v;
        const float cx = boatX_, cy = boatY_ + bob;
        const RGB hull = RGB{110, 46, 34} + landFlash * 0.4f, cabin = RGB{70, 70, 74} + landFlash * 0.6f;
        for (int dx = -8; dx <= 8; ++dx) {
            const float lift = tilt * dx;
            for (int dy = 0; dy < 4; ++dy)
                if (std::abs(dx) <= 8 - dy * 1.6f)
                    buf_.set(static_cast<int>(cx + dx), static_cast<int>(std::floor(cy + dy + lift)), dy == 0 ? hull * 1.3f : hull);
        }
        for (int dx = 1; dx <= 5; ++dx)
            for (int dy = -3; dy < 0; ++dy) buf_.set(static_cast<int>(cx + dx), static_cast<int>(std::floor(cy + dy + tilt * dx)), cabin);
        buf_.set(static_cast<int>(cx + 4), static_cast<int>(std::floor(cy - 2 + tilt * 4)), lampC * 0.9f);
        // Broken reflection of the cabin light below the hull.
        for (int k = 4; k < 12; ++k) {
            const float flick = hash(static_cast<uint32_t>(k * 97 + static_cast<int>(time_ * 8.0f) * 13));
            if (flick > 0.35f)
                buf_.add(static_cast<int>(cx + 4 + (flick - 0.6f) * 2.0f), static_cast<int>(cy + k), lampC * (0.25f * (1.0f - k / 12.0f)));
        }
        const float mx = cx - 3.0f;
        for (int k = 1; k <= 11; ++k)
            buf_.set(static_cast<int>(mx + tilt * k * -1.0f), static_cast<int>(std::floor(cy - k + tilt * -3.0f)),
                     RGB{40, 38, 40} + landFlash * 0.5f);
        const float topX = mx - tilt * 11.0f, topY = cy - 11.0f - tilt * 3.0f;
        glow(buf_, topX + 0.5f, topY + 0.5f, 3.0f, RGB{255, 240, 220} * 0.35f);
        buf_.set(static_cast<int>(topX), static_cast<int>(std::floor(topY)), RGB{255, 245, 230});
    }

    // --- rain streaks --------------------------------------------------------------------------------------------------
    const float q = render_quality_scale();
    const int active = static_cast<int>(rain_.size() * q * std::clamp((0.30f + treble * 1.1f) * 0.5f * rainAmount_->get(), 0.0f, 1.0f));
    const float slant = 0.25f + 0.0002f * std::fmod(wind_, 1000.0f);
    for (int i = 0; i < active; ++i) {
        auto& r = rain_[static_cast<size_t>(i)];
        r.y += dt * (70.0f + 70.0f * r.speed);
        r.x += dt * (70.0f + 70.0f * r.speed) * slant;
        if (r.y > H) {
            r.y -= H;
            r.x = rnd() * W;
        }
        if (r.x > W)
            r.x -= W;
        const float a = 0.22f * r.speed + 0.5f * lightning;
        line(buf_, r.x, r.y, r.x - slant * 3.0f, r.y - 3.0f, RGB{150, 165, 220} * a);
    }

    // Whole-frame flash and low-frequency rumble.
    if (lightning > 0.02f)
        for (auto& p : buf_.px) p = p + RGB{100, 105, 150} * (0.07f * lightning);
    // The strike lights the sky and the sea around it more than the far side.
    for (const auto& b : bolts_) glow(buf_, b.x, static_cast<float>(hz), 46.0f, RGB{120, 125, 175} * (0.30f * b.life));
    if (kick_.v > 0.02f)
        for (auto& p : buf_.px) p = p * (1.0f + kick_.v * 0.08f);
    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioStormFrontScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "storm";
    d.tags = {"music",         "rock",           "metal",     "storm",     "weather", "coast",          "sea",
              "village",       "fits:storm",     "fits:rain", "lightning", "night",   "audio-reactive", "beat-driven",
              "drop-reactive", "music-showcase", "style:rock"};
    d.intensity = 0.72f;
    d.motion = 0.55f;
    d.music_affinity = 0.95f;
    d.performance_cost = 0.55f;
    return d;
}
}  // namespace Scenes
