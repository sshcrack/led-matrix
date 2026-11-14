#include "matrix_control/MatrixPresenter.h"
#include "matrix_control/SceneRenderer.h"

#include <shared/common/timesource/TimeSource.h>
#include <shared/matrix/Scene.h>
#include <shared/matrix/diagnostics.h>
#include <shared/matrix/utils/utils.h>
#include <shared/matrix/utils/shared.h>

#include <emulator.h>
#include <led-matrix.h>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace {

class FakeTimeSource final : public TimeSource {
public:
    std::int64_t now_ms() override { return now_us_ / 1000; }
    std::int64_t now_us() override { return now_us_; }
    void sleep_us(std::int64_t us) override { now_us_ += us; }
    void advance(std::int64_t ms) { now_us_ += ms * 1000; }
private:
    std::int64_t now_us_ = 1000;
};

class VSyncTimingMatrix final : public rgb_matrix::RGBMatrixBase {
public:
    VSyncTimingMatrix(rgb_matrix::EmulatorMatrix *matrix, FakeTimeSource *time_source)
        : matrix_(matrix), time_source_(time_source) {}

    int width() const override { return matrix_->width(); }
    int height() const override { return matrix_->height(); }
    void SetPixel(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) override {
        matrix_->SetPixel(x, y, r, g, b);
    }
    void Clear() override { matrix_->Clear(); }
    void Fill(std::uint8_t r, std::uint8_t g, std::uint8_t b) override { matrix_->Fill(r, g, b); }

    rgb_matrix::FrameCanvas *CreateFrameCanvas() override { return matrix_->CreateFrameCanvas(); }
    rgb_matrix::FrameCanvas *SwapOnVSync(rgb_matrix::FrameCanvas *other, unsigned fraction = 1) override {
        auto *result = matrix_->SwapOnVSync(other, fraction);
        time_source_->advance(16 * static_cast<std::int64_t>(fraction));
        return result;
    }
    bool SetPWMBits(std::uint8_t value) override { return matrix_->SetPWMBits(value); }
    std::uint8_t pwmbits() override { return matrix_->pwmbits(); }
    void SetBrightness(std::uint8_t value) override { matrix_->SetBrightness(value); }
    std::uint8_t brightness() override { return matrix_->brightness(); }
    void set_luminance_correct(bool on) override { matrix_->set_luminance_correct(on); }
    bool luminance_correct() const override { return matrix_->luminance_correct(); }
    bool StartRefresh() override { return matrix_->StartRefresh(); }

private:
    rgb_matrix::EmulatorMatrix *matrix_;
    FakeTimeSource *time_source_;
};

class SixFrameScene final : public Scenes::Scene {
public:
    bool render(rgb_matrix::FrameCanvas *canvas) override {
        ++render_count_;
        canvas->Fill(80, 120, 160);
        return render_count_ < 6;
    }
    void register_properties() override {}
    std::string get_name() const override { return "cadence-probe"; }
protected:
    tmillis_t get_default_duration() override { return 2000; }
    int get_default_weight() override { return 1; }
private:
    int render_count_ = 0;
};

class CountingPresenter final : public MatrixPresenter {
public:
    void present() override { ++count; }
    int count = 0;
};

} // namespace

int main()
{
    // Exercise the real wait used by the renderer when VSync leaves a short
    // remainder. The fake clock below advances waits without real sleeping. Use a
    // median and generous scheduling tolerance rather than one tight deadline.
    std::vector<double> short_waits;
    for (int i = 0; i < 15; ++i) {
        const auto start = std::chrono::steady_clock::now();
        SleepMillis(3);
        short_waits.push_back(std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count());
    }
    std::sort(short_waits.begin(), short_waits.end());
    const auto median_wait = short_waits[short_waits.size() / 2];
    if (median_wait < 2.0 || median_wait >= 8.0) {
        std::cerr << "3 ms pacing wait rounded to " << median_wait << " ms\n";
        return 5;
    }
    for (auto *flag : {&skip_image, &exit_canvas_update}) {
        flag->store(true);
        const auto start = std::chrono::steady_clock::now();
        SleepMillis(500);
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        const bool skip_consumed = !skip_image.load();
        flag->store(false);
        if (elapsed > 80.0 || !skip_consumed) {
            std::cerr << "long wait ignored an interrupt\n";
            return 6;
        }
    }

    rgb_matrix::RGBMatrix::Options led;
    led.rows = 32;
    led.cols = 32;
    led.chain_length = 1;
    led.parallel = 1;

    rgb_matrix::EmulatorOptions emulator;
    emulator.headless = true;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> backing(
        rgb_matrix::EmulatorMatrix::Create(led, emulator));
    if (!backing) {
        std::cerr << "failed to create emulator matrix\n";
        return 1;
    }

    FakeTimeSource time_source;
    VSyncTimingMatrix matrix(backing.get(), &time_source);
    auto scene = std::make_shared<SixFrameScene>();
    scene->update_default_properties();
    scene->register_properties();
    scene->load_properties(nlohmann::json::object());
    scene->initialize(32, 32);

    auto *canvas = matrix.CreateFrameCanvas();
    CountingPresenter presenter;
    std::atomic<bool> exit_flag{false};
    std::atomic<bool> interrupt_flag{false};
    FrameSink sink(&matrix, &time_source, nullptr, &presenter);
    SceneRenderer renderer(&matrix, &time_source, &sink, &exit_flag, &interrupt_flag);

    const auto wall_start = std::chrono::steady_clock::now();
    renderer.render_scene_phase(scene, canvas, 5000);
    const double elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - wall_start).count();

    if (presenter.count != 6) {
        std::cerr << "expected six presentations, got " << presenter.count << '\n';
        return 2;
    }

    // Six starts at 0, 16.667, ..., 83.334 ms plus the final 16 ms
    // VSync wait. Integer-16-ms or jitter-relative pacing finishes too soon.
    if (time_source.now_us() != 100334) {
        std::cerr << "renderer lost fractional frame deadlines: " << time_source.now_us() << '\n';
        return 7;
    }

    const auto diagnostics = Diagnostics::RuntimeDiagnostics::instance().snapshot();
    const auto &renderer_stats = diagnostics.at("renderer");
    if (renderer_stats.value("presentation_frames", 0ULL) != 6ULL
        || !renderer_stats.contains("presentation_interval_ms_last")
        || !renderer_stats.contains("late_presentations")
        || !renderer_stats.contains("estimated_missed_refreshes")) {
        std::cerr << "presentation deadline telemetry was not recorded\n";
        return 4;
    }

    // The fake SwapOnVSync advances scene time by one 16 ms refresh interval.
    // Fractional remainders are advanced by the fake clock, without real sleep on
    // top. The historical double-pacing bug sleeps ~20 ms between frames.
    if (elapsed_ms > 35.0) {
        std::cerr << "renderer added a second pacing wait: " << elapsed_ms << "ms for six frames\n";
        return 3;
    }

    std::cout << "six frames completed in " << elapsed_ms << "ms wall time\n";
    return 0;
}
