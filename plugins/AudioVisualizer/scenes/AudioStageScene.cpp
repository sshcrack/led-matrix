#include "AmbientMusicScenes.h"

#include <array>

namespace Scenes {
using namespace AudioKit;

void AudioStageScene::register_properties()
{
    add_property(sensitivity_);
    add_property(crowdSize_);
    add_property(strobe_);
}

bool AudioStageScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    spectrum_.update(audio, dt, 0.05f, 0.25f);
    if (lightLevel_.empty())
        lightLevel_.assign(Lights, 0.0f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    const float snareF = feature(audio, AudioProtocol::Feature::Snare);
    const float energy = spectrum_.level;
    if (events_.beatHit)
        beat_.hit();
    if (events_.onsetHit && kickF > 0.35f)
        kick_.hit(0.5f + 0.5f * kickF);
    if (events_.onsetHit && snareF > 0.35f)
        snare_.hit(snareF);
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.18f);
    kick_.decay(dt, 0.16f);
    snare_.decay(dt, 0.12f);
    drop_.decay(dt, 1.6f);
    palette_.update(dt, 1.2f);
    const float bpm = std::clamp(audio.feature(AudioProtocol::Feature::Bpm), 70.0f, 180.0f);
    sweep_ += dt * (bpm / 60.0f) * 0.25f * (0.6f + energy * 0.8f);

    const int stageY = static_cast<int>(H * 0.60f);  // stage floor edge
    const int trussY = 9;

    // --- backdrop: dark arena, haze lit by the overall energy -------------
    const RGB haze = hsv(palette_.value, 0.7f, 1.0f);
    for (int y = 0; y < H; ++y) {
        const float t = static_cast<float>(y) / H;
        const RGB base = lerp(RGB{4, 3, 10}, RGB{10, 6, 18}, t);
        for (int x = 0; x < W; ++x) buf_.at(x, y) = base + haze * (0.025f + 0.04f * energy) * (1.0f - t);
    }

    // --- moving-head beams: one light per band ----------------------------
    struct Beam {
        float lx, dx, dy, width;
        RGB col;
    };
    std::array<Beam, Lights> beams{};
    for (int i = 0; i < Lights; ++i) {
        const float from = static_cast<float>(i) / Lights, to = static_cast<float>(i + 1) / Lights;
        const float target = spectrum_.band(from, to);
        auto& lv = lightLevel_[static_cast<size_t>(i)];
        lv += (target - lv) * (1.0f - std::exp(-dt / 0.08f));
        const float lx = (i + 0.5f) / Lights * W;
        const float side = i < Lights / 2 ? 1.0f : -1.0f;
        // Mirrored sweeps; on a drop every beam snaps to centre stage.
        float angle = std::sin(sweep_ * 2.0f + i * 0.9f) * 0.55f * side + std::sin(sweep_ * 0.7f + i) * 0.15f;
        angle = angle * (1.0f - drop_.v) + (W * 0.5f - lx) / H * 0.9f * drop_.v;
        const float hue = palette_.value + (i % 2 == 0 ? 0.0f : 35.0f);
        const RGB col = hsv(hue, i % 3 == 1 ? 0.25f : 0.85f, 1.0f);
        const float power = std::clamp(0.10f + lv * 0.85f + beat_.v * 0.15f + drop_.v * 0.4f, 0.0f, 1.2f);
        beams[static_cast<size_t>(i)] = {lx, std::sin(angle), std::cos(angle), 0.045f + 0.035f * lv, col * (power * 0.75f)};
        // Fixture head on the truss.
        buf_.disc(lx, trussY + 1.0f, 1.6f, col * (0.5f + 0.5f * power));
    }
    // All beams as one smooth layer (coarser lattice at reduced Pi quality).
    const float top = static_cast<float>(trussY);
    buf_.add_field(trussY + 1, H, field_step(), [&](float x, float y) {
        RGB sum;
        for (const auto& b : beams) {
            const float px = x - b.lx, py = y - top;
            const float along = px * b.dx + py * b.dy;
            if (along <= 0)
                continue;
            const float cone = std::fabs(px * b.dy - py * b.dx) / (along * b.width + 1.0f);
            if (cone > 1.6f)
                continue;
            sum = sum + b.col * std::exp(-cone * cone * 2.2f - along / 140.0f);
        }
        return sum;
    });

    // --- truss --------------------------------------------------------------
    for (int x = 0; x < W; ++x) {
        buf_.set(x, trussY - 2, RGB{40, 40, 48});
        buf_.set(x, trussY, RGB{40, 40, 48});
        if ((x / 3) % 2 == 0)
            buf_.set(x, trussY - 1, RGB{28, 28, 34});
    }

    // --- stage: floor, amps, band silhouettes (backlit) ----------------------
    for (int y = stageY; y < stageY + 4; ++y)
        for (int x = 4; x < W - 4; ++x) buf_.set(x, y, RGB{14, 12, 18} + haze * (0.12f + 0.2f * beat_.v));
    for (int a = 0; a < 2; ++a) {  // amp stacks
        const int ax = a == 0 ? 8 : W - 22;
        for (int y = stageY - 18; y < stageY; ++y)
            for (int x = ax; x < ax + 14; ++x) buf_.set(x, y, (y - stageY) % 9 == 0 ? RGB{30, 30, 34} : RGB{10, 10, 12});
    }
    const auto person = [&](float cx, float lean, float height, bool guitar) {
        const float head_top = stageY - height;
        buf_.disc(cx + lean * 2.0f, head_top + 2.5f, 2.6f, RGB{0, 0, 0}, false);  // head
        for (int y = static_cast<int>(head_top + 5); y < stageY; ++y) {
            const float k = (y - head_top - 5) / (height - 5);
            const float half = k < 0.45f ? 3.2f : 1.6f + (k - 0.45f) * 2.0f;
            const float sway = lean * (1.0f - k) * 2.0f;
            for (int x = static_cast<int>(cx + sway - half); x <= static_cast<int>(cx + sway + half); ++x) buf_.set(x, y, RGB{0, 0, 0});
        }
        if (guitar)  // neck of the guitar, angled
            for (int t = 0; t < 12; ++t)
                buf_.set(static_cast<int>(cx - 3 + t), static_cast<int>(head_top + 13 - t * 0.45f + lean), RGB{0, 0, 0});
    };
    const float headbang = beat_.v;
    person(W * 0.30f, -0.6f + headbang * 1.2f, 25.0f, true);
    person(W * 0.50f, std::sin(time_ * 1.3f) * 0.4f + snare_.v * 0.6f, 27.0f, false);  // singer
    // Mic stand.
    for (int y = stageY - 20; y < stageY; ++y) buf_.set(static_cast<int>(W * 0.50f) + 5, y, RGB{0, 0, 0});
    person(W * 0.70f, 0.6f - headbang * 1.2f, 24.0f, true);
    // Drum kit behind: kick drum face glows on kicks.
    buf_.disc(W * 0.50f, stageY - 6.0f, 5.5f, RGB{0, 0, 0}, false);
    buf_.disc(W * 0.50f, stageY - 6.0f, 4.0f, haze * (0.2f + 0.6f * kick_.v), true);

    // --- crowd silhouettes in front, jumping on kicks --------------------------
    const int rows = std::max(0, static_cast<int>(std::round(2 * crowdSize_->get())));
    if (crowd_.empty() && rows > 0) {
        for (int r = 0; r < 4; ++r)
            for (int i = 0; i < 14; ++i)
                crowd_.push_back({i * 10.0f + hash(r * 31 + i) * 6.0f - 3.0f + (r % 2) * 5.0f, static_cast<float>(r), hash(r * 7 + i * 13) * 6.28f,
                                  hash(r * 17 + i * 3)});
    }
    for (auto it = crowd_.rbegin(); it != crowd_.rend(); ++it) {
        const auto& c = *it;
        const int row = static_cast<int>(c.y);
        if (row >= rows + 1)
            continue;
        const float baseY = H + 4.0f - row * 8.0f;
        const float jump = (kick_.v * (0.6f + 0.4f * std::sin(c.phase)) + drop_.v * 0.6f) * (5.0f - row) * 0.9f;
        const float y = baseY - 16.0f - jump;
        const RGB shade = RGB{1, 1, 3} + haze * (0.03f * row);
        // Backlit rim, then the silhouette: head, neck, rounded shoulders.
        // Only the back row catches the stage light; front rows stay black.
        const RGB rim = row == rows ? haze * (0.30f + 0.30f * beat_.v) : shade;
        buf_.disc(c.x, y - 0.6f, 4.3f, rim, false);
        buf_.disc(c.x, y, 3.8f, shade, false);
        for (int yy = static_cast<int>(y + 3); yy < H; ++yy) {
            const float k = std::min(1.0f, (yy - y - 3) / 4.0f);
            const float half = 2.0f + 4.5f * std::sqrt(k);
            for (int xx = static_cast<int>(c.x - half); xx <= static_cast<int>(c.x + half); ++xx) buf_.set(xx, yy, shade);
        }
        // Raised hands at loud moments.
        if (c.hand < 0.25f + energy * 0.5f + drop_.v * 0.4f) {
            const float hx = c.x + (c.hand < 0.5f ? -3.0f : 3.0f);
            for (int t = 0; t < 11; ++t) {
                const int ax = static_cast<int>(hx + (c.hand - 0.5f) * t * 0.6f), ay = static_cast<int>(y - t);
                buf_.set(ax, ay, shade);
                buf_.set(ax + 1, ay, shade);
            }
        }
    }

    // --- blinder on drops: one warm flash that fades out. No flicker:
    // fast strobing (3-30 Hz) is a photosensitivity risk in a room.
    if (strobe_->get() && drop_.v > 0.05f) {
        const float a = drop_.v * drop_.v * 0.30f;
        for (auto& p : buf_.px) p = p + RGB{255, 245, 225} * a;
    }

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioStageScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "stage";
    d.tags = {"music", "concert", "stage", "night", "spectrum", "audio-reactive", "beat-driven", "music-showcase", "style:rock",
              "style:party", "drop-reactive"};
    d.intensity = 0.72f;
    d.motion = 0.70f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.45f;
    return d;
}
}  // namespace Scenes
