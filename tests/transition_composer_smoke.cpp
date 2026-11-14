// The transition composition rules both hosts share (the Pi's TransitionEngine and the
// desktop scene worker): name resolution with the "blend" fallback, smoothstep easing,
// the missing-incoming-frame fallback and the outgoing policy.
#include <emulator.h>
#include <led-matrix.h>
#include <shared/matrix/transition_composer.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    if (!ok) {
        std::cerr << "FAILED: " << what << '\n';
        ++failures;
    }
}

std::uint8_t red(rgb_matrix::FrameCanvas* canvas)
{
    std::uint8_t r = 0, g = 0, b = 0;
    canvas->GetPixel(0, 0, &r, &g, &b);
    return r;
}

/// Records what the composer asked of it and writes `marker` so a test can tell effects apart.
class RecordingEffect final : public TransitionEffect {
public:
    RecordingEffect(std::string effect_name, std::uint8_t marker) : name_(std::move(effect_name)), marker_(marker) {}
    std::string get_name() const override { return name_; }
    void apply(FrameCanvas* dst, FrameCanvas* from, FrameCanvas* to, float alpha, int, int) override
    {
        ++calls;
        last_alpha = alpha;
        last_from = red(from);
        last_to = red(to);
        dst->Fill(marker_, marker_, marker_);
    }
    int calls = 0;
    float last_alpha = -1.0f;
    std::uint8_t last_from = 0;
    std::uint8_t last_to = 0;

private:
    std::string name_;
    std::uint8_t marker_;
};

}  // namespace

int main()
{
    rgb_matrix::RGBMatrix::Options led;
    led.rows = 16;
    led.cols = 16;
    led.chain_length = 1;
    led.parallel = 1;
    rgb_matrix::EmulatorOptions emulator;
    emulator.headless = true;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> matrix(rgb_matrix::EmulatorMatrix::Create(led, emulator));
    if (!matrix) {
        std::cerr << "failed to create emulator matrix\n";
        return 1;
    }
    auto* from = matrix->CreateFrameCanvas();
    auto* to = matrix->CreateFrameCanvas();
    auto* out = matrix->CreateFrameCanvas();
    from->Fill(40, 40, 40);
    to->Fill(200, 200, 200);

    // Name resolution: the named effect, else "blend", else the incoming frame untouched.
    {
        TransitionManager manager;
        auto blend = std::make_unique<RecordingEffect>("blend", 11);
        auto special = std::make_unique<RecordingEffect>("special", 22);
        auto* blend_raw = blend.get();
        auto* special_raw = special.get();
        manager.register_transition(std::move(blend));
        manager.register_transition(std::move(special));

        TransitionComposer composer(&manager);
        check(composer.begin(from, "special", 1000, 16, 16, 0), "known name resolves");
        composer.frame(out, to, true, 100);
        check(special_raw->calls == 1 && blend_raw->calls == 0 && red(out) == 22, "the named effect composes");

        check(composer.begin(from, "no-such-effect", 1000, 16, 16, 0), "unknown name falls back to blend");
        composer.frame(out, to, true, 100);
        check(blend_raw->calls == 1 && red(out) == 11, "the blend effect composes after the fallback");

        TransitionManager only_special;
        only_special.register_transition(std::make_unique<RecordingEffect>("special", 22));
        TransitionComposer without_blend(&only_special);
        check(!without_blend.begin(from, "no-such-effect", 1000, 16, 16, 0), "no blend, no effect");
        without_blend.frame(out, to, true, 100);
        check(red(out) == 200, "without any effect the incoming frame is shown as is");

        TransitionComposer without_manager(nullptr);
        check(!without_manager.begin(from, "blend", 1000, 16, 16, 0), "no manager, no effect");
        without_manager.frame(out, to, true, 100);
        check(red(out) == 200, "without a manager the incoming frame is shown as is");
    }

    // Easing and completion.
    {
        TransitionManager manager;
        auto effect = std::make_unique<RecordingEffect>("blend", 11);
        auto* recorded = effect.get();
        manager.register_transition(std::move(effect));
        TransitionComposer composer(&manager);
        composer.begin(from, "blend", 1000, 16, 16, 5000);
        check(composer.blending() && composer.active(), "begin starts a blend");

        const auto start = composer.frame(out, to, true, 5000);
        check(recorded->last_alpha == 0.0f && !start.done, "progress starts at 0");
        const auto quarter = composer.frame(out, to, true, 5250);
        check(std::fabs(recorded->last_alpha - TransitionComposer::ease(0.25f)) < 1e-6f && std::fabs(quarter.eased - 0.15625f) < 1e-6f,
              "progress is smoothstepped (0.25 -> 0.15625)");
        check(std::fabs(composer.eased_progress(5500) - 0.5f) < 1e-6f, "eased_progress reads without composing");
        check(recorded->calls == 2, "eased_progress composed nothing");
        const auto before = composer.frame(out, to, true, 4000);
        check(recorded->last_alpha == 0.0f && !before.done, "progress clamps below 0");
        const auto end = composer.frame(out, to, true, 6000);
        check(recorded->last_alpha == 1.0f && end.done && end.eased == 1.0f, "the final frame is composed at 1 and reports done");
        check(!composer.active() && !composer.blending(), "the blend ends once the incoming scene has a frame");
    }

    // A zero-length transition still completes.
    {
        TransitionManager manager;
        manager.register_transition(std::make_unique<RecordingEffect>("blend", 11));
        TransitionComposer composer(&manager);
        composer.begin(from, "blend", 0, 16, 16, 100);
        check(composer.frame(out, to, true, 101).done, "duration 0 is treated as 1 ms");
    }

    // An incoming scene without a frame yet blends from the outgoing snapshot.
    {
        TransitionManager manager;
        auto effect = std::make_unique<RecordingEffect>("blend", 11);
        auto* recorded = effect.get();
        manager.register_transition(std::move(effect));
        TransitionComposer composer(&manager);
        composer.begin(from, "blend", 1000, 16, 16, 0);
        to->Fill(99, 99, 99);  // whatever the reusable buffer held
        composer.frame(out, to, false, 100);
        check(recorded->last_to == 40, "an incoming canvas without a frame is replaced by the outgoing snapshot");
        to->Fill(200, 200, 200);
        composer.frame(out, to, true, 200);
        check(recorded->last_to == 200, "the first real incoming frame is used as is");
        to->Fill(210, 210, 210);
        composer.frame(out, to, false, 300);
        check(recorded->last_to == 210, "later calls keep using the incoming canvas even when it holds a frame");

        // The blend ends before the incoming scene ever painted: stay on the snapshot.
        TransitionComposer late(&manager);
        late.begin(from, "blend", 100, 16, 16, 0);
        to->Fill(99, 99, 99);
        const auto done = late.frame(out, to, false, 500);
        check(done.done && red(to) == 40, "a blend that ends without an incoming frame ends on the snapshot");
        check(late.active() && !late.blending(), "that composer stays active until the incoming scene paints");
        to->Fill(200, 200, 200);
        late.frame(out, to, true, 600);
        check(!late.active(), "and releases once it has");
    }

    // The outgoing policy: the Pi freezes the snapshot, the worker may keep it moving.
    {
        TransitionManager manager;
        manager.register_transition(std::make_unique<RecordingEffect>("blend", 11));
        auto* newer = matrix->CreateFrameCanvas();
        newer->Fill(77, 77, 77);

        from->Fill(40, 40, 40);
        TransitionComposer freeze(&manager, TransitionComposer::OutgoingPolicy::Freeze);
        check(freeze.outgoing_policy() == TransitionComposer::OutgoingPolicy::Freeze, "freeze is the default");
        freeze.begin(from, "blend", 1000, 16, 16, 0);
        check(!freeze.refresh_outgoing(*newer) && red(from) == 40, "Freeze keeps the snapshot");

        TransitionComposer moving(&manager, TransitionComposer::OutgoingPolicy::KeepMoving);
        moving.begin(from, "blend", 1000, 16, 16, 0);
        check(moving.refresh_outgoing(*newer) && red(from) == 77, "KeepMoving takes newer outgoing frames");
        moving.frame(out, to, true, 1000);
        newer->Fill(88, 88, 88);
        check(!moving.refresh_outgoing(*newer) && red(from) == 77, "a finished blend ignores further outgoing frames");
        moving.end();
        check(!moving.active() && !moving.refresh_outgoing(*newer), "end() drops the blend");
    }

    if (failures == 0)
        std::cout << "ok\n";
    return failures == 0 ? 0 : 1;
}
