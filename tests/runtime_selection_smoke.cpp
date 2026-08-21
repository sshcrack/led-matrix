#include "matrix_control/LiveFrameSnapshot.h"
#include "matrix_control/MatrixPresenter.h"
#include "matrix_control/SceneRenderer.h"
#include "matrix_control/SceneScheduler.h"

#include <shared/common/timesource/TimeSource.h>
#include <shared/matrix/Scene.h>
#include <shared/matrix/input_ids.h>
#include <shared/matrix/runtime_inputs.h>

#include <emulator.h>
#include <led-matrix.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {

class SteppingTimeSource final : public TimeSource {
public:
    explicit SteppingTimeSource(std::int64_t step_ms) : step_ms_(step_ms) {}
    std::int64_t now_ms() override {
        now_ms_ += step_ms_;
        return now_ms_;
    }
private:
    std::int64_t now_ms_ = 0;
    std::int64_t step_ms_ = 0;
};

class TestScene final : public Scenes::Scene {
public:
    TestScene(std::string name, std::string required_input = {})
        : name_(std::move(name)), required_input_(std::move(required_input)) {}

    bool render(rgb_matrix::FrameCanvas *canvas) override {
        ++render_count;
        canvas->SetPixel(0, 0, 1, 2, 3);
        return true;
    }
    void register_properties() override {}
    std::string get_name() const override { return name_; }
    Scenes::SceneInputSpec get_runtime_input_spec() const override {
        Scenes::SceneInputSpec spec;
        if (!required_input_.empty()) spec.require(required_input_);
        return spec;
    }

    int render_count = 0;

protected:
    tmillis_t get_default_duration() override { return 2000; }
    int get_default_weight() override { return 10; }

private:
    std::string name_;
    std::string required_input_;
};

std::shared_ptr<TestScene> make_scene(std::string name, std::string required_input = {}) {
    auto scene = std::make_shared<TestScene>(std::move(name), std::move(required_input));
    scene->update_default_properties();
    scene->register_properties();
    scene->load_properties(nlohmann::json::object());
    return scene;
}

} // namespace

int main() {
    RuntimeInputs::clear_all();

    SceneScheduler scheduler;
    auto ambient = make_scene("ambient");
    auto audio = make_scene("audio", std::string(RuntimeInputIds::Audio));
    const std::vector<std::shared_ptr<Scenes::Scene>> scenes{ambient, audio};

    auto weighted = scheduler.build_weighted_scenes(scenes, RuntimeInputs::snapshot());
    if (weighted.size() != 1 || weighted.front().scene != ambient) {
        std::cerr << "scheduler did not exclude a scene with a missing required input\n";
        return 1;
    }

    RuntimeInputs::publish(RuntimeInputIds::Audio, {}, std::chrono::seconds(1));
    weighted = scheduler.build_weighted_scenes(scenes, RuntimeInputs::snapshot());
    if (weighted.size() != 2) {
        std::cerr << "scheduler did not admit a scene after its required input appeared\n";
        return 2;
    }

    rgb_matrix::RGBMatrix::Options led;
    led.rows = 32;
    led.cols = 32;
    led.chain_length = 1;
    led.parallel = 1;
    rgb_matrix::EmulatorOptions emulator;
    emulator.headless = true;
    emulator.refresh_rate_hz = 60;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> matrix(rgb_matrix::EmulatorMatrix::Create(led, emulator));
    if (!matrix) {
        std::cerr << "failed to create emulator matrix\n";
        return 3;
    }

    audio->initialize(32, 32);
    auto *canvas = matrix->CreateFrameCanvas();
    NoopPresenter presenter(matrix.get());
    SteppingTimeSource time_source(100);
    std::atomic<bool> exit_flag{false};
    std::atomic<bool> interrupt_flag{false};
    SceneRenderer renderer(matrix.get(), &time_source, nullptr, &presenter, &exit_flag, &interrupt_flag);

    const auto input_spec = audio->get_effective_runtime_inputs();
    int availability_checks = 0;
    const bool early_exit = renderer.render_scene_phase(
        audio, canvas, 5000,
        [&] {
            ++availability_checks;
            const bool available = RuntimeInputs::satisfies(input_spec, RuntimeInputs::snapshot());
            if (availability_checks == 1)
                RuntimeInputs::set_available(RuntimeInputIds::Audio, false);
            return available;
        });

    if (!early_exit || availability_checks < 2 || audio->render_count == 0) {
        std::cerr << "renderer did not retire an active scene after required input loss\n";
        return 4;
    }

    RuntimeInputs::clear_all();
    return 0;
}
