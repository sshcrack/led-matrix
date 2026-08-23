#include "canvas.h"

#include "shared/matrix/utils/utils.h"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/diagnostics.h"
#include "SceneLabRuntime.h"
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
    , automatic_director_(cfg->get_automatic_director_seed())
    , renderer_(matrix, time_source, post_processor, presenter, exit_flag, interrupt_flag)
    , transition_engine_(matrix, time_source, post_processor, transition_manager, presenter, exit_flag, interrupt_flag)
{
    first_offscreen_canvas_ = matrix->CreateFrameCanvas();
    second_offscreen_canvas_ = matrix->CreateFrameCanvas();
    composite_offscreen_canvas_ = matrix->CreateFrameCanvas();
}

CanvasCoordinator::~CanvasCoordinator() = default;

void CanvasCoordinator::ensure_automatic_catalog()
{
    if (!automatic_scenes_.empty()) return;

    automatic_preset_ = std::make_shared<ConfigData::Preset>();
    automatic_preset_->transition_duration = 750;
    automatic_preset_->transition_name = "blend";
    automatic_preset_->display_name = "Automatic";

    for (const auto &wrapper : Plugins::PluginManager::instance()->get_scenes()) {
        if (!wrapper) continue;
        const auto descriptor = wrapper->get_default()->get_descriptor();
        if (!descriptor.automatic_eligible) continue;

        auto create_instance = [&](const std::string &variant_id) {
            auto instance = wrapper->create();
            instance->update_default_properties();
            instance->register_properties();
            if (!variant_id.empty()) instance->apply_variant(variant_id);
            else instance->load_properties(nlohmann::json::object());
            automatic_scenes_.push_back(std::shared_ptr<Scenes::Scene>(std::move(instance)));
        };

        if (descriptor.variants.empty()) {
            create_instance("");
        } else {
            for (const auto &variant : descriptor.variants) create_instance(variant.id);
        }
    }
    info("Automatic Director catalog contains {} curated scene looks", automatic_scenes_.size());
}

void CanvasCoordinator::run(std::shared_ptr<Scenes::Scene> pinned_scene)
{
    const auto configured_seed = config_->get_automatic_director_seed();
    const auto configured_generation = config_->get_automatic_director_generation();
    if (automatic_director_.seed() != configured_seed
        || automatic_director_generation_ != configured_generation) {
        automatic_director_.reseed(configured_seed);
        automatic_director_generation_ = configured_generation;
        info("Automatic Director reseeded with {}", configured_seed);
    }
    Diagnostics::RuntimeDiagnostics::instance().set_director_state(automatic_director_.diagnostics());

    const auto lab_snapshot = SceneLabRuntime::instance().snapshot(runtime_inputs_fn_());
    const bool lab_mode = !pinned_scene && lab_snapshot.active && lab_snapshot.scene;
    const bool automatic_mode = !lab_mode && !pinned_scene && config_->is_automatic_mode();
    std::shared_ptr<ConfigData::Preset> preset;
    std::vector<std::shared_ptr<Scenes::Scene>> lab_scenes;
    const std::vector<std::shared_ptr<Scenes::Scene>> *scene_source = nullptr;
    if (lab_mode) {
        lab_scenes = {lab_snapshot.scene};
        preset = std::make_shared<ConfigData::Preset>();
        preset->display_name = "Scene Lab";
        preset->transition_duration = 0;
        preset->transition_name = "blend";
        scene_source = &lab_scenes;
    } else if (automatic_mode) {
        ensure_automatic_catalog();
        preset = automatic_preset_;
        scene_source = &automatic_scenes_;
    } else {
        preset = config_->get_curr();
        if (!preset) {
            error("config->get_curr() returned null, using fallback preset");
            preset = ConfigData::Preset::create_default();
        }
        scene_source = &preset->scenes;
    }

    // A coordinator run restarts when configuration/runtime mode changes.
    // Never carry a transition-preselected scene across that mode boundary.
    forced_scene_.reset();

    const auto &scenes = *scene_source;
    const int matrix_width = matrix_->width();
    const int matrix_height = matrix_->height();

    auto &first = first_offscreen_canvas_;
    auto &second = second_offscreen_canvas_;
    auto &composite = composite_offscreen_canvas_;

    int no_scene_count = 0;
    while (!*exit_flag_) {
        if (lab_mode && !SceneLabRuntime::instance().lease_active(lab_snapshot.generation))
            return;
        const auto runtime_inputs = runtime_inputs_fn_();

        std::shared_ptr<Scenes::Scene> scene = lab_mode
            ? lab_snapshot.scene
            : (pinned_scene ? pinned_scene : forced_scene_);
        std::string exclude_name = scene ? scene->get_name() : "";
        forced_scene_ = nullptr;

        if (scene == nullptr) {
            if (automatic_mode) {
                const auto decision = automatic_director_.choose(scenes, runtime_inputs, exclude_name);
                scene = decision.scene;
                Diagnostics::RuntimeDiagnostics::instance().set_director_state(automatic_director_.diagnostics());
            } else {
                auto weighted = scheduler_.build_weighted_scenes(scenes, runtime_inputs, exclude_name);
                scene = scheduler_.select_scene(weighted);
            }
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

        if (lab_mode) {
            const auto missing = RuntimeInputs::missing_required(
                scene->get_effective_runtime_inputs(), runtime_inputs_fn_());
            if (!missing.empty()) {
                set_curr_scene_fn_(nullptr);
                renderer_.render_fallback();
                presenter_->present();
                SleepMillis(250);
                continue;
            }
        }

        no_scene_count = 0;
        if (!scene->is_initialized()) scene->initialize(matrix_width, matrix_height);
        const tmillis_t presentation_duration = lab_mode
            ? 3600000
            : (automatic_mode
                ? automatic_director_.presentation_duration(scene, runtime_inputs)
                : scene->get_duration());
        if (automatic_mode) automatic_director_.record_played(scene);
        const tmillis_t end_ms = time_source_->now_ms() + presentation_duration;

        set_curr_scene_fn_(scene);

        debug("Now displaying scene: {}", scene->get_name());
        broadcast_fn_(scene->get_name());

        std::shared_ptr<Scenes::Scene> next_scene;
        auto transition_duration =
            scheduler_.resolve_transition_duration(preset, scene);
        auto transition_name =
            scheduler_.resolve_transition_name(preset, scene);
        if (scheduler_.should_schedule_transition(transition_duration, presentation_duration)
            && !pinned_scene && !automatic_mode && !lab_mode) {
            auto weighted = scheduler_.build_weighted_scenes(scenes, runtime_inputs,
                scene != nullptr ? scene->get_name() : "");
            next_scene = scheduler_.select_scene(weighted);
            if (next_scene != nullptr && !next_scene->is_initialized())
                next_scene->initialize(matrix_width, matrix_height);
        }

        std::function<bool()> inputs_still_available;
        if (!pinned_scene) {
            const auto input_spec = scene->get_effective_runtime_inputs();
            if (lab_mode) {
                const auto generation = lab_snapshot.generation;
                inputs_still_available = [this, input_spec, generation] {
                    return SceneLabRuntime::instance().lease_active(generation)
                        && RuntimeInputs::satisfies(input_spec, runtime_inputs_fn_());
                };
            } else if (!input_spec.required.empty()) {
                inputs_still_available = [this, input_spec] {
                    return RuntimeInputs::satisfies(input_spec, runtime_inputs_fn_());
                };
            }
        }

        bool early_exit = renderer_.render_scene_phase(
            scene, composite, end_ms, std::move(inputs_still_available));

        if (!early_exit && automatic_mode && !lab_mode
            && scheduler_.should_schedule_transition(transition_duration, presentation_duration)) {
            const auto latest_inputs = runtime_inputs_fn_();
            const auto decision = automatic_director_.choose(scenes, latest_inputs, scene->get_name());
            next_scene = decision.scene;
            Diagnostics::RuntimeDiagnostics::instance().set_director_state(automatic_director_.diagnostics());
            if (next_scene != nullptr && !next_scene->is_initialized())
                next_scene->initialize(matrix_width, matrix_height);
        }

        if (!early_exit && !lab_mode && next_scene != nullptr
            && RuntimeInputs::satisfies(
                next_scene->get_effective_runtime_inputs(), runtime_inputs_fn_())) {
            tmillis_t transition_delay = 0;
            if (automatic_mode) {
                const auto plan = transition_planner_.plan(
                    scene, next_scene, transition_duration, transition_name, AudioState::snapshot());
                transition_duration = plan.duration_ms;
                transition_name = plan.name;
                transition_delay = plan.start_delay_ms;
                Diagnostics::RuntimeDiagnostics::instance().set_transition_state({
                    {"from", scene->get_name()}, {"from_variant", scene->get_variant_id()},
                    {"to", next_scene->get_name()}, {"to_variant", next_scene->get_variant_id()},
                    {"name", plan.name}, {"duration_ms", plan.duration_ms},
                    {"start_delay_ms", plan.start_delay_ms},
                    {"beat_synchronized", plan.beat_synchronized}, {"reason", plan.reason}
                });
                debug("Automatic transition {} -> {}: {} ms {}, delay {} ms ({})",
                    scene->get_name(), next_scene->get_name(), transition_duration, transition_name,
                    transition_delay, plan.reason);
            }
            const auto current_input_spec = scene->get_effective_runtime_inputs();
            const auto next_input_spec = next_scene->get_effective_runtime_inputs();
            std::function<bool()> transition_inputs_still_available;
            if (!current_input_spec.required.empty() || !next_input_spec.required.empty()) {
                transition_inputs_still_available = [this, current_input_spec, next_input_spec] {
                    const auto snapshot = runtime_inputs_fn_();
                    return RuntimeInputs::satisfies(current_input_spec, snapshot)
                        && RuntimeInputs::satisfies(next_input_spec, snapshot);
                };
            }
            transition_engine_.render_transition_phase(
                scene, next_scene,
                first, second, composite,
                matrix_width, matrix_height,
                transition_duration, transition_name, forced_scene_, transition_delay,
                std::move(transition_inputs_still_available), renderer_.last_presented_canvas());
        }

        if (automatic_mode) {
            automatic_director_.report_render_quality(scene->get_render_quality_scale());
            Diagnostics::RuntimeDiagnostics::instance().set_director_state(automatic_director_.diagnostics());
        }
        scene->after_render_stop();
    }
}
