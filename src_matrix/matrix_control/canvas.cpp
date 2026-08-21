#include "canvas.h"

#include "shared/matrix/utils/utils.h"
#include "spdlog/spdlog.h"

using namespace std;
using namespace spdlog;

CanvasCoordinator::CanvasCoordinator(RGBMatrixBase *matrix,
                                      TimeSource *time_source,
                                      PostProcessor *post_processor,
                                      TransitionManager *transition_manager,
                                      MatrixPresenter *presenter,
                                      Config::MainConfig *cfg,
                                      const std::atomic<bool> *exit_flag,
                                      const std::atomic<bool> *interrupt_flag,
                                      std::function<RuntimeInputs::Snapshot()> runtime_inputs,
                                      std::function<void(std::shared_ptr<Scenes::Scene>)> set_curr_scene,
                                      std::function<void(const std::string &)> broadcast)
    : matrix_(matrix)
    , time_source_(time_source)
    , presenter_(presenter)
    , config_(cfg)
    , exit_flag_(exit_flag)
    , interrupt_flag_(interrupt_flag)
    , runtime_inputs_fn_(std::move(runtime_inputs))
    , set_curr_scene_fn_(std::move(set_curr_scene))
    , broadcast_fn_(std::move(broadcast))
    , renderer_(matrix, time_source, post_processor, presenter, exit_flag, interrupt_flag)
    , transition_engine_(matrix, time_source, post_processor, transition_manager, presenter, exit_flag, interrupt_flag)
{
    first_offscreen_canvas_ = matrix->CreateFrameCanvas();
    second_offscreen_canvas_ = matrix->CreateFrameCanvas();
    composite_offscreen_canvas_ = matrix->CreateFrameCanvas();
}

CanvasCoordinator::~CanvasCoordinator() = default;

void CanvasCoordinator::run(std::shared_ptr<Scenes::Scene> pinned_scene)
{
    std::shared_ptr<ConfigData::Preset> preset = config_->get_curr();
    if (!preset) {
        error("config->get_curr() returned null, using fallback preset");
        preset = ConfigData::Preset::create_default();
    }

    const auto &scenes = preset->scenes;
    const int matrix_width = matrix_->width();
    const int matrix_height = matrix_->height();

    for (const auto &item : scenes) {
        if (!item->is_initialized())
            item->initialize(matrix_width, matrix_height);
    }

    auto &first = first_offscreen_canvas_;
    auto &second = second_offscreen_canvas_;
    auto &composite = composite_offscreen_canvas_;

    int no_scene_count = 0;
    while (!*exit_flag_) {
        const auto runtime_inputs = runtime_inputs_fn_();

        std::shared_ptr<Scenes::Scene> scene =
            pinned_scene ? pinned_scene : forced_scene_;
        std::string exclude_name = scene ? scene->get_name() : "";
        forced_scene_ = nullptr;

        if (scene == nullptr) {
            auto weighted = scheduler_.build_weighted_scenes(scenes, runtime_inputs, exclude_name);
            scene = scheduler_.select_scene(weighted);
        }

        if (scene == nullptr) {
            if (no_scene_count < 3)
                error("Could not find scene to display.");
            no_scene_count++;

            set_curr_scene_fn_(nullptr);
            renderer_.render_fallback();

            presenter_->present();

            SleepMillis(300);
            continue;
        }

        no_scene_count = 0;
        const tmillis_t end_ms = time_source_->now_ms() + scene->get_duration();

        set_curr_scene_fn_(scene);

        debug("Now displaying scene: {}", scene->get_name());
        broadcast_fn_(scene->get_name());

        std::shared_ptr<Scenes::Scene> next_scene;
        const auto transition_duration =
            scheduler_.resolve_transition_duration(preset, scene);
        const auto transition_name =
            scheduler_.resolve_transition_name(preset, scene);
        if (scheduler_.should_schedule_transition(transition_duration, scene->get_duration())
            && !pinned_scene) {
            auto weighted = scheduler_.build_weighted_scenes(scenes, runtime_inputs,
                scene != nullptr ? scene->get_name() : "");
            next_scene = scheduler_.select_scene(weighted);
            if (next_scene != nullptr && !next_scene->is_initialized())
                next_scene->initialize(matrix_width, matrix_height);
        }

        bool early_exit = renderer_.render_scene_phase(scene, composite, end_ms);

        if (!early_exit && next_scene != nullptr) {
            transition_engine_.render_transition_phase(
                scene, next_scene,
                first, second, composite,
                matrix_width, matrix_height,
                transition_duration, transition_name, forced_scene_);
        }

        scene->after_render_stop();
    }
}
