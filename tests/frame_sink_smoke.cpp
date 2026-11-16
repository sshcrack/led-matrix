#include <emulator.h>
#include <led-matrix.h>
#include <shared/common/timesource/TimeSource.h>
#include <shared/matrix/Scene.h>
#include <shared/matrix/transition_manager.h>

#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "matrix_control/FrameSink.h"
#include "matrix_control/LiveFrameSnapshot.h"
#include "matrix_control/MatrixPresenter.h"
#include "matrix_control/SceneRenderer.h"
#include "matrix_control/TransitionEngine.h"

namespace {

class SteppingTimeSource final : public TimeSource {
public:
    std::int64_t now_ms() override { return now_ms_ += 20; }

private:
    std::int64_t now_ms_ = 0;
};

// Paints one frame with its colour, then holds it like a paused media scene.
class FillOnceScene final : public Scenes::Scene {
public:
    explicit FillOnceScene(std::uint8_t value) : value_(value) {}
    bool render(rgb_matrix::FrameCanvas* canvas) override
    {
        if (rendered_) {
            hold_current_frame();
            return true;
        }
        rendered_ = true;
        canvas->Fill(value_, value_, value_);
        return true;
    }
    void register_properties() override {}
    std::string get_name() const override { return "fill-once"; }

protected:
    tmillis_t get_default_duration() override { return 2000; }
    int get_default_weight() override { return 1; }

private:
    std::uint8_t value_;
    bool rendered_ = false;
};

// One frame, then the scene ends: drives SceneRenderer::render_scene_phase().
class OneFrameScene final : public Scenes::Scene {
public:
    explicit OneFrameScene(std::uint8_t value) : value_(value) {}
    bool render(rgb_matrix::FrameCanvas* canvas) override
    {
        canvas->Fill(value_, value_, value_);
        return ++renders_ < 1;
    }
    void register_properties() override {}
    std::string get_name() const override { return "one-frame"; }

protected:
    tmillis_t get_default_duration() override { return 2000; }
    int get_default_weight() override { return 1; }

private:
    std::uint8_t value_;
    int renders_ = 0;
};

class LinearBlend final : public TransitionEffect {
public:
    std::string get_name() const override { return "blend"; }
    void apply(FrameCanvas* dst, FrameCanvas* from, FrameCanvas* to, float alpha, int width, int height) override
    {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                std::uint8_t fr = 0, fg = 0, fb = 0, tr = 0, tg = 0, tb = 0;
                from->GetPixel(x, y, &fr, &fg, &fb);
                to->GetPixel(x, y, &tr, &tg, &tb);
                const auto mix = [alpha](std::uint8_t a, std::uint8_t b) { return static_cast<std::uint8_t>(a + (b - a) * alpha); };
                dst->SetPixel(x, y, mix(fr, tr), mix(fg, tg), mix(fb, tb));
            }
        }
    }
};

// Stands in for the real matrix: forwards to the emulator and records what the
// sink does to the panel, in order.
class RecordingMatrix final : public rgb_matrix::RGBMatrixBase {
public:
    explicit RecordingMatrix(rgb_matrix::EmulatorMatrix* matrix) : matrix_(matrix) {}

    int width() const override { return matrix_->width(); }
    int height() const override { return matrix_->height(); }
    void SetPixel(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) override { matrix_->SetPixel(x, y, r, g, b); }
    void Clear() override { matrix_->Clear(); }
    void Fill(std::uint8_t r, std::uint8_t g, std::uint8_t b) override { matrix_->Fill(r, g, b); }
    rgb_matrix::FrameCanvas* CreateFrameCanvas() override { return matrix_->CreateFrameCanvas(); }
    rgb_matrix::FrameCanvas* SwapOnVSync(rgb_matrix::FrameCanvas* other, unsigned fraction = 1) override
    {
        events.push_back("swap");
        return matrix_->SwapOnVSync(other, fraction);
    }
    bool SetPWMBits(std::uint8_t value) override { return matrix_->SetPWMBits(value); }
    std::uint8_t pwmbits() override { return matrix_->pwmbits(); }
    void SetBrightness(std::uint8_t value) override
    {
        events.push_back("brightness=" + std::to_string(value));
        matrix_->SetBrightness(value);
    }
    std::uint8_t brightness() override { return matrix_->brightness(); }
    void set_luminance_correct(bool on) override { matrix_->set_luminance_correct(on); }
    bool luminance_correct() const override { return matrix_->luminance_correct(); }
    bool StartRefresh() override { return matrix_->StartRefresh(); }

    std::vector<std::string> events;

private:
    rgb_matrix::EmulatorMatrix* matrix_;
};

class RecordingPresenter final : public MatrixPresenter {
public:
    RecordingPresenter(rgb_matrix::EmulatorMatrix* matrix, RecordingMatrix* log) : matrix_(matrix), log_(log) {}
    void present() override
    {
        log_->events.push_back("present");
        std::uint8_t r = 0, g = 0, b = 0;
        if (matrix_->GetPixel(0, 0, &r, &g, &b))
            samples.push_back(r);
    }
    std::vector<std::uint8_t> samples;

private:
    rgb_matrix::EmulatorMatrix* matrix_;
    RecordingMatrix* log_;
};

std::shared_ptr<Scenes::Scene> make_scene(std::uint8_t value)
{
    auto scene = std::make_shared<FillOnceScene>(value);
    scene->update_default_properties();
    scene->register_properties();
    scene->load_properties(nlohmann::json::object());
    scene->initialize(32, 32);
    return scene;
}

}  // namespace

int fail(const std::string& message)
{
    std::cerr << message << '\n';
    return 1;
}

int main()
{
    rgb_matrix::RGBMatrix::Options led;
    led.rows = 32;
    led.cols = 32;
    led.chain_length = 1;
    led.parallel = 1;
    rgb_matrix::EmulatorOptions emulator;
    emulator.headless = true;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> backing(rgb_matrix::EmulatorMatrix::Create(led, emulator));
    if (!backing)
        return fail("failed to create emulator matrix");

    RecordingMatrix matrix(backing.get());
    auto* first = matrix.CreateFrameCanvas();
    auto* second = matrix.CreateFrameCanvas();
    auto* composite = matrix.CreateFrameCanvas();

    RecordingPresenter presenter(backing.get(), &matrix);
    SteppingTimeSource time_source;
    TransitionManager transitions;
    transitions.register_transition(std::make_unique<LinearBlend>());
    std::atomic<bool> exit_flag{false};
    std::atomic<bool> interrupt_flag{false};
    FrameSink sink(&matrix, &time_source, nullptr, &presenter);
    SceneRenderer renderer(&matrix, &time_source, &sink, &exit_flag, &interrupt_flag);
    TransitionEngine engine(&time_source, &transitions, &sink, &exit_flag, &interrupt_flag);

    // 1. One present(): swap, then the presenter, and the sink knows what is on the panel.
    if (sink.visible() != nullptr)
        return fail("sink claims a visible frame before presenting one");
    {
        auto* drawn = composite;
        drawn->Fill(77, 77, 77);
        sink.present(composite, 60);
        if (sink.visible() != drawn || composite == drawn)
            return fail("present() did not report the presented canvas as visible");
        if (matrix.events != std::vector<std::string>{"swap", "present"})
            return fail("present() did not swap and then notify the presenter");
        if (presenter.samples.back() != 77)
            return fail("presenter ran before the frame was on the panel");
    }

    // 2. Back-to-back transitions with no scene frame in between. The stale
    // last-presented canvas used to be handed to the second transition as "the
    // frame on the panel"; the double buffer makes it wrong for half the frame
    // counts, so sweep both parities.
    for (const tmillis_t duration : {100, 120, 140, 160}) {
        auto a = make_scene(50);
        auto b = make_scene(150);
        auto c = make_scene(150);  // same colour: a correct blend stays flat
        auto phase = std::make_shared<OneFrameScene>(50);
        phase->update_default_properties();
        phase->register_properties();
        phase->load_properties(nlohmann::json::object());
        phase->initialize(32, 32);
        renderer.render_scene_phase(phase, composite, 5000);

        // Transition one: A -> B. B paints its single frame, then holds.
        (void)engine.render_transition_phase(a, b, first, second, composite, 32, 32, duration, "blend", 0, {});
        // Transition two follows with no scene frame in between. B only holds
        // its frame now, so the outgoing snapshot has to be what is on the
        // panel: the final frame of transition one (B).
        presenter.samples.clear();
        (void)engine.render_transition_phase(b, c, first, second, composite, 32, 32, duration, "blend", 0, {});
        if (presenter.samples.empty())
            return fail("second transition presented nothing");
        for (const auto sample : presenter.samples) {
            if (sample != 150) {
                std::cerr << "second transition (" << duration << " ms) started from a stale canvas:";
                for (const auto value : presenter.samples) std::cerr << ' ' << static_cast<int>(value);
                std::cerr << '\n';
                return 2;
            }
        }
    }

    // 3. The brightness schedule applies to frames presented by a transition.
    {
        std::optional<int> scheduled = 40;
        sink.set_brightness_provider([&] { return scheduled; });
        matrix.events.clear();
        auto a = make_scene(50);
        auto b = make_scene(150);
        (void)engine.render_transition_phase(a, b, first, second, composite, 32, 32, 100, "blend", 0, {});
        if (backing->brightness() != 40 || matrix.events.empty() || matrix.events.front() != "brightness=40")
            return fail("transition frames skipped the brightness schedule");
        sink.set_brightness_provider({});
    }

    // 4. Live capture is pull-based: nothing is captured until a viewer asks,
    // one request is served by exactly one captured frame, and then it is idle again.
    {
        int captured = 0;
        LiveFrame::SnapshotStore::instance().set_publish_callback([&](LiveFrame::Snapshot, std::uint64_t) { ++captured; });
        auto a = make_scene(50);
        auto b = make_scene(150);
        (void)engine.render_transition_phase(a, b, first, second, composite, 32, 32, 100, "blend", 0, {});
        if (captured != 0)
            return fail("a frame was captured without a viewer request");

        (void)LiveFrame::SnapshotStore::instance().request_capture();
        auto c = make_scene(50);
        auto d = make_scene(150);
        (void)engine.render_transition_phase(c, d, first, second, composite, 32, 32, 100, "blend", 0, {});
        if (captured != 1)
            return fail("one viewer request was not served by exactly one captured frame");
        LiveFrame::SnapshotStore::instance().set_publish_callback({});
    }
    return 0;
}
