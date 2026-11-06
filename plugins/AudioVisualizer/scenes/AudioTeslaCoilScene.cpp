#include "GenreVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioTeslaCoilScene::register_properties()
{
    add_property(sensitivity_);
    add_property(coils_);
    add_property(arcRate_);
}

void AudioTeslaCoilScene::spark(float x0, float y0, float x1, float y1, float hue)
{
    Arc a;
    a.pts = {{x0, y0}, {x1, y1}};
    a.life = 1.0f;
    a.hue = hue;
    arcs_.push_back(std::move(a));
    if (arcs_.size() > 14)
        arcs_.erase(arcs_.begin());
}

bool AudioTeslaCoilScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    spectrum_.update(audio, dt, 0.03f, 0.14f);

    const int nc = coils_->get();
    const float ground = H * 0.86f;
    const float topY = H * 0.30f;
    auto coilX = [&](int i) { return W * (static_cast<float>(i) + 0.5f) / nc; };
    auto rodX = [&](int i) { return W * (static_cast<float>(i) + 1.0f) / nc; };  // rods sit between the coils

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    const float bass = spectrum_.band(0.0f, 0.2f);
    const float treble = spectrum_.band(0.5f, 1.0f);
    const float rate = arcRate_->get();
    if (events_.onsetHit && kickF > 0.3f) {
        kick_.hit(kickF);
        const int c = static_cast<int>(rnd() * nc);
        const int r = std::min(nc - 1, std::max(0, c + (rnd() < 0.5f ? -1 : 0)));
        spark(coilX(c), topY, rodX(r) + (rnd() - 0.5f) * 4.0f, ground - 10.0f, palette_.value);
    }
    if (events_.beatHit)
        beat_.hit(0.5f + 0.5f * kickF);
    // Sustained bass keeps a fizzing arc going.
    if (rnd() < dt * 14.0f * bass * rate) {
        const int c = static_cast<int>(rnd() * nc);
        spark(coilX(c), topY, W * rnd(), ground, palette_.value + 20.0f);
    }
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
        for (int i = 0; i + 1 < nc; ++i) spark(coilX(i), topY, coilX(i + 1), topY, palette_.value);
        for (int i = 0; i < nc; ++i) spark(coilX(i), topY, W * rnd(), H * 0.1f + H * 0.5f * rnd(), palette_.value + 40.0f);
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.2f);
    kick_.decay(dt, 0.12f);
    drop_.decay(dt, 1.6f);
    corona_.v += (treble - corona_.v) * (1.0f - std::exp(-dt / 0.08f));
    palette_.update(dt, 1.0f);
    for (auto& a : arcs_) a.life -= dt / 0.22f;
    std::erase_if(arcs_, [](const Arc& a) { return a.life <= 0.0f; });

    const RGB accent = hsv(palette_.value, 0.7f, 1.0f);
    // --- dark lab with a slightly lit concrete floor ------------------------------------------------------------
    vgradient(buf_, 0, static_cast<int>(ground), RGB{4, 4, 9}, RGB{12, 10, 20});
    for (int y = static_cast<int>(ground); y < H; ++y)
        for (int x = 0; x < W; ++x)
            buf_.at(x, y) = RGB{14, 13, 17} * (0.7f + 0.3f * hash(static_cast<uint32_t>(y * 313 + x))) * (1.0f - 0.4f * (y - ground) / (H - ground));
    glow(buf_, W * 0.5f, topY, W * 0.9f, accent * (0.05f + 0.10f * drop_.v + 0.07f * corona_.v));

    // --- ground rods ----------------------------------------------------------------------------------------------
    for (int i = 0; i < nc; ++i) {
        const float x = rodX(i);
        for (int y = static_cast<int>(ground) - 10; y < static_cast<int>(ground); ++y) buf_.set(static_cast<int>(x), y, RGB{110, 110, 120});
        buf_.disc(x, ground - 10.0f, 1.5f, RGB{170, 170, 180}, false);
    }

    // --- coils ----------------------------------------------------------------------------------------------------
    for (int i = 0; i < nc; ++i) {
        const float x = coilX(i);
        // Base and winding with windings that glow warm as the coil works.
        rect(buf_, static_cast<int>(x) - 7, static_cast<int>(ground) - 5, static_cast<int>(x) + 8, static_cast<int>(ground), RGB{36, 36, 42});
        const int w0 = static_cast<int>(ground) - 6, w1 = static_cast<int>(topY) + 6;
        for (int y = w1; y < w0; ++y) {
            const float t = static_cast<float>(y - w1) / (w0 - w1);
            const float half = 3.5f + 3.0f * t;
            const bool ring = y % 2 == 0;
            const RGB cu = ring ? RGB{190, 105, 55} : RGB{90, 48, 28};
            for (int xx = static_cast<int>(x - half); xx <= static_cast<int>(x + half); ++xx)
                buf_.set(xx, y, cu * (0.5f + 0.5f * std::cos((xx - x) / half * 1.2f)) * (0.7f + 0.8f * (bass + kick_.v) * (1.0f - t)));
        }
        // Toroid on top.
        ellipse(buf_, x, topY, 9.0f, 4.5f, RGB{150, 155, 170});
        ellipse(buf_, x, topY + 0.5f, 5.0f, 2.0f, RGB{18, 18, 24});
        buf_.add(static_cast<int>(x - 4), static_cast<int>(topY - 3), RGB{230, 235, 255});
        // Corona and streamers fed by the treble.
        glow(buf_, x, topY, 12.0f + 10.0f * corona_.v, accent * (0.30f + 0.9f * corona_.v + 0.5f * kick_.v));
        const int streamers = static_cast<int>(3 + corona_.v * 12 * rate);
        for (int s = 0; s < streamers; ++s) {
            const float ang = -Pi * (0.05f + 0.9f * rnd());
            float px = x + std::cos(ang) * 8.0f, py = topY + std::sin(ang) * 4.0f;
            const float l = 4.0f + 10.0f * corona_.v * rnd();
            for (int k = 0; k < 4; ++k) {
                const float nx = px + std::cos(ang + (rnd() - 0.5f) * 1.2f) * l * 0.25f;
                const float ny = py + std::sin(ang + (rnd() - 0.5f) * 1.2f) * l * 0.25f;
                line(buf_, px, py, nx, ny, accent * (0.9f - 0.2f * k));
                px = nx;
                py = ny;
            }
        }
    }

    // --- main arcs: jagged midpoint-displaced bolts, re-jittered every frame so they crackle -------------
    for (const auto& a : arcs_) {
        const float fade = std::clamp(a.life, 0.0f, 1.0f);
        const RGB col = hsv(a.hue, 0.35f, 1.0f) * (1.5f * fade);
        std::vector<std::pair<float, float>> pts{a.pts.front(), a.pts.back()};
        float disp = len(pts[1].first - pts[0].first, pts[1].second - pts[0].second) * 0.18f;
        for (int pass = 0; pass < 4; ++pass) {
            std::vector<std::pair<float, float>> next;
            for (size_t i = 0; i + 1 < pts.size(); ++i) {
                next.push_back(pts[i]);
                next.push_back({(pts[i].first + pts[i + 1].first) * 0.5f + (rnd() - 0.5f) * disp,
                                (pts[i].second + pts[i + 1].second) * 0.5f + (rnd() - 0.5f) * disp});
            }
            next.push_back(pts.back());
            pts = std::move(next);
            disp *= 0.55f;
        }
        for (size_t i = 1; i < pts.size(); ++i) line(buf_, pts[i - 1].first, pts[i - 1].second, pts[i].first, pts[i].second, col);
        glow(buf_, a.pts.back().first, a.pts.back().second, 9.0f, accent * (0.45f * fade));
    }
    if (drop_.v > 0.02f)
        for (auto& p : buf_.px) p = p + accent * (0.12f * drop_.v);
    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioTeslaCoilScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "tesla-coil";
    d.tags = {"music", "electro", "techno", "lightning", "lab", "audio-reactive", "beat-driven", "drop-reactive",
              "music-showcase", "style:edm", "style:rock"};
    d.intensity = 0.88f;
    d.motion = 0.78f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.50f;
    return d;
}
}  // namespace Scenes
