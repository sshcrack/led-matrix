#include "matrix_control/MatrixPresenter.h"
#include "matrix_control/TransitionEngine.h"

#include <shared/common/timesource/TimeSource.h>
#include <shared/matrix/Scene.h>

#include <emulator.h>
#include <led-matrix.h>

#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace {

class SteppingTimeSource final : public TimeSource {
public:
    std::int64_t now_ms() override {
        now_ms_ += 20;
        return now_ms_;
    }
private:
    std::int64_t now_ms_ = 0;
};

class HeldScene final : public Scenes::Scene {
public:
    bool render(rgb_matrix::FrameCanvas *) override {
        hold_current_frame();
        return true;
    }
    void register_properties() override {}
    std::string get_name() const override { return "transition-held"; }
protected:
    tmillis_t get_default_duration() override { return 2000; }
    int get_default_weight() override { return 1; }
};

class StableScene final : public Scenes::Scene {
public:
    bool render(rgb_matrix::FrameCanvas *canvas) override {
        canvas->Fill(212, 212, 212);
        return true;
    }
    void register_properties() override {}
    std::string get_name() const override { return "transition-stable"; }
protected:
    tmillis_t get_default_duration() override { return 2000; }
    int get_default_weight() override { return 1; }
};

class RecordingPresenter final : public MatrixPresenter {
public:
    explicit RecordingPresenter(rgb_matrix::EmulatorMatrix *matrix) : matrix_(matrix) {}

    void present() override {
        std::uint8_t r = 0, g = 0, b = 0;
        if (matrix_->GetPixel(0, 0, &r, &g, &b))
            samples.push_back(r);
    }

    std::vector<std::uint8_t> samples;

private:
    rgb_matrix::EmulatorMatrix *matrix_;
};

std::shared_ptr<Scenes::Scene> initialize_scene(std::shared_ptr<Scenes::Scene> scene)
{
    scene->update_default_properties();
    scene->register_properties();
    scene->load_properties(nlohmann::json::object());
    scene->initialize(32, 32);
    return scene;
}

} // namespace

int main()
{
    rgb_matrix::RGBMatrix::Options led;
    led.rows = 32;
    led.cols = 32;
    led.chain_length = 1;
    led.parallel = 1;

    rgb_matrix::EmulatorOptions emulator;
    emulator.headless = true;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> matrix(
        rgb_matrix::EmulatorMatrix::Create(led, emulator));
    if (!matrix) {
        std::cerr << "failed to create emulator matrix\n";
        return 1;
    }

    // Put a known bright frame on the actual display. Every reusable transition
    // canvas starts dark so an erroneous swap is immediately visible.
    auto *display = matrix->CreateFrameCanvas();
    display->Fill(212, 212, 212);
    (void)matrix->SwapOnVSync(display, 1);

    auto *first = matrix->CreateFrameCanvas();
    auto *second = matrix->CreateFrameCanvas();
    auto *composite = matrix->CreateFrameCanvas();
    first->Fill(18, 18, 18);
    second->Fill(18, 18, 18);
    composite->Fill(18, 18, 18);

    auto current = initialize_scene(std::make_shared<HeldScene>());
    auto next = initialize_scene(std::make_shared<StableScene>());

    RecordingPresenter presenter(matrix.get());
    SteppingTimeSource time_source;
    std::atomic<bool> exit_flag{false};
    std::atomic<bool> interrupt_flag{false};
    TransitionEngine engine(
        matrix.get(), &time_source, nullptr, nullptr, &presenter,
        &exit_flag, &interrupt_flag);

    std::shared_ptr<Scenes::Scene> forced_scene;
    engine.render_transition_phase(
        current, next, first, second, composite,
        32, 32, 100, "blend", forced_scene, 100, {}, display);

    if (presenter.samples.empty()) {
        std::cerr << "transition produced no presented frames\n";
        return 2;
    }
    for (const auto sample : presenter.samples) {
        if (sample != 212) {
            std::cerr << "held transition exposed stale off-screen buffer: ";
            for (const auto value : presenter.samples)
                std::cerr << static_cast<int>(value) << ' ';
            std::cerr << '\n';
            return 3;
        }
    }

    return 0;
}
