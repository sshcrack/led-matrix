#include "matrix_control/MatrixPresenter.h"
#include "matrix_control/SceneRenderer.h"

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

class HoldFrameScene final : public Scenes::Scene {
public:
    bool render(rgb_matrix::FrameCanvas *canvas) override {
        ++render_count_;
        if (render_count_ == 1)
            canvas->Fill(18, 18, 18);
        else if (render_count_ == 2)
            canvas->Fill(212, 212, 212);
        else
            hold_current_frame();
        // After frame two, model a paused media scene: remain active without
        // producing a new visual frame.
        return render_count_ < 6;
    }

    void register_properties() override {}
    std::string get_name() const override { return "hold-frame"; }
    int render_count() const { return render_count_; }

protected:
    tmillis_t get_default_duration() override { return 2000; }
    int get_default_weight() override { return 1; }

private:
    int render_count_ = 0;
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

    auto scene = std::make_shared<HoldFrameScene>();
    scene->update_default_properties();
    scene->register_properties();
    scene->load_properties(nlohmann::json::object());
    scene->initialize(32, 32);

    auto *canvas = matrix->CreateFrameCanvas();
    RecordingPresenter presenter(matrix.get());
    SteppingTimeSource time_source;
    std::atomic<bool> exit_flag{false};
    std::atomic<bool> interrupt_flag{false};
    SceneRenderer renderer(
        matrix.get(), &time_source, nullptr, &presenter,
        &exit_flag, &interrupt_flag);

    renderer.render_scene_phase(scene, canvas, 5000);

    if (scene->render_count() != 6) {
        std::cerr << "renderer stopped polling a held scene: " << scene->render_count() << " renders\n";
        return 2;
    }

    // Only the two frames that actually changed may be presented. The four
    // held iterations must leave the second frame latched on the matrix.
    if (presenter.samples != std::vector<std::uint8_t>{18, 212}) {
        std::cerr << "held frame was presented from a stale double buffer: ";
        for (const auto sample : presenter.samples)
            std::cerr << static_cast<int>(sample) << ' ';
        std::cerr << '\n';
        return 3;
    }

    std::uint8_t r = 0, g = 0, b = 0;
    if (!matrix->GetPixel(0, 0, &r, &g, &b) || r != 212 || g != 212 || b != 212) {
        std::cerr << "held frame did not remain latched on the active matrix\n";
        return 4;
    }

    return 0;
}
