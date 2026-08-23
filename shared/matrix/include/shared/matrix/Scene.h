#pragma once

#include "nlohmann/json.hpp"
#include "fmt/format.h"
#include "led-matrix.h"
#include <spdlog/spdlog.h>
#include <vector>
#include <shared/matrix/plugin/property.h>
#include <shared/common/utils/utils.h>
#include <shared/matrix/plugin/PropertyMacros.h>
#include <shared/matrix/plugin/TransitionNameProperty.h>
#include <shared/matrix/scene_runtime.h>
#include <shared/matrix/scene_descriptor.h>
#include <shared/matrix/input_ids.h>
#include <shared/matrix/preview.h>
#include <chrono>
#include <optional>
#include <unordered_map>

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::RGBMatrixBase;
using std::string;

template <typename T>
using Property = Plugins::Property<T>;
namespace Scenes {
    class Scene {
        std::vector<std::shared_ptr<Plugins::PropertyBase>> properties;

    protected:
        bool initialized = false;
        int matrix_width;
        int matrix_height;
        int target_fps = 60;
        tmillis_t last_render_time = 0;
        SceneFrameContext frame_context_{};
        std::chrono::steady_clock::time_point frame_clock_start_{};
        std::chrono::steady_clock::time_point frame_clock_last_{};
        bool frame_clock_started_ = false;
        bool suppress_internal_wait_ = false;
        double frame_wait_ms_ = 0.0;
        float render_quality_scale_ = 1.0f;
        unsigned render_over_budget_streak_ = 0;
        unsigned render_under_budget_streak_ = 0;

        virtual int get_default_weight() = 0;
        virtual tmillis_t get_default_duration() = 0;

        // Initialize with temporary values instead of calling virtual functions
        PropertyPointer<int> weight = MAKE_PROPERTY("weight", int, 1);
        PropertyPointer<tmillis_t> duration = MAKE_PROPERTY("duration", tmillis_t, 5000);
        PropertyPointer<tmillis_t> transition_duration = MAKE_PROPERTY("transition_duration", tmillis_t, 0);
        std::shared_ptr<Plugins::TransitionNameProperty> transition_name = std::make_shared<Plugins::TransitionNameProperty>();
        PropertyPointer<nlohmann::json> audio_modulations_ = MAKE_PROPERTY(
            "audio_modulations", nlohmann::json, nlohmann::json::array());

        struct AudioModulationState {
            double base_value = 0.0;
            double smoothed_value = 0.0;
        };
        std::unordered_map<std::string, AudioModulationState> audio_modulation_state_;

        void apply_audio_modulations(double dt);
        void restore_audio_modulations();

        std::string uuid;
        std::string variant_id_;

        void set_target_fps(int fps) {
            target_fps = fps > 0 ? fps : 1;
        }

        [[nodiscard]] int get_target_fps() const {
            return target_fps;
        }

        virtual void wait_until_next_frame();

        /// Current render-frame timing. Prefer this over reading wall clock time
        /// inside a scene. The matrix renderer and preview generator populate it.
        [[nodiscard]] const SceneFrameContext &frame_context() const { return frame_context_; }

        /// Adaptive quality scale for scenes with optional expensive detail.
        /// 1.0 means full quality. Values below 1.0 indicate sustained CPU pressure.
        [[nodiscard]] float render_quality_scale() const { return render_quality_scale_; }

        /// Give expensive scenes a conservative startup quality. The adaptive
        /// controller can still recover all the way to 1.0 when the device has
        /// enough headroom.
        void set_render_quality_hint(float scale) {
            render_quality_scale_ = std::clamp(scale, 0.45f, 1.0f);
        }

        /// Reset timing when a scene is reinitialized or reused.
        void reset_frame_clock();

        void add_property(const std::shared_ptr<Plugins::PropertyBase> &property) {
            std::string name = property->getName();
            for (const auto &item: properties) {
                if (item->getName() == name) {
                    throw std::runtime_error(fmt::format("Property with name '{}' already exists", name));
                }
            }

            properties.push_back(property);
        }

    public:
        Scene();

        virtual ~Scene() = default;  // Changed to proper virtual destructor with default implementation

        [[nodiscard]] std::string get_uuid() const {
            return this->uuid;
        }

        [[nodiscard]] const SceneFrameContext &get_frame_context() const { return frame_context_; }
        void restore_frame_timeline(double elapsed_seconds) {
            frame_context_.elapsed_seconds = std::max(0.0, elapsed_seconds);
            frame_context_.now_ms = static_cast<std::uint64_t>(frame_context_.elapsed_seconds * 1000.0);
            frame_clock_started_ = false;
        }


        /// This method is used to update the default of properties dynamically. It is called before a property has been registered.
        virtual void update_default_properties() {
            spdlog::trace("Updating default properties for scene '{}'", get_name());
            weight->set_value(get_default_weight());
            duration->set_value(get_default_duration());
        }

        [[nodiscard]] int get_declared_target_fps() const { return target_fps; }
        [[nodiscard]] double get_last_frame_wait_ms() const { return frame_wait_ms_; }
        [[nodiscard]] float get_render_quality_scale() const { return render_quality_scale_; }
        void report_render_cost(double active_render_ms);

        [[nodiscard]] virtual int get_weight() const;

        [[nodiscard]] virtual tmillis_t get_duration() const;

        [[nodiscard]] virtual tmillis_t get_transition_duration() const;

        [[nodiscard]] virtual std::string get_transition_name() const;

        [[nodiscard]] virtual nlohmann::json to_json() const;

        /// Optional transient state used when placement migrates between the
        /// Pi and desktop worker. Most deterministic scenes need no override;
        /// stateful scenes can opt in without changing the transport protocol.
        [[nodiscard]] virtual nlohmann::json snapshot_runtime_state() const {
            return nlohmann::json::object();
        }
        virtual void restore_runtime_state(const nlohmann::json &) {}

        [[nodiscard]] virtual string get_name() const = 0;
        [[nodiscard]] virtual std::string get_category() const { return "General"; }

        /// Stable high-level metadata consumed by directors and the default UI.
        /// Scene-specific rendering knobs intentionally do not leak through here.
        [[nodiscard]] virtual SceneDescriptor get_descriptor() const;
        [[nodiscard]] const std::string &get_variant_id() const { return variant_id_; }
        void apply_variant(std::string_view id);
        void set_external_variant_id(std::string id) { variant_id_ = std::move(id); }
        void set_runtime_target_fps(int fps) { set_target_fps(fps); }

        /// Declarative preview contract. Desktop-dependent scenes are disabled
        /// by default, but may opt in by requesting fixture inputs supplied by
        /// plugin-owned preview data providers.
        [[nodiscard]] virtual Previews::SceneSpec get_preview_spec() const {
            return const_cast<Scene *>(this)->needs_desktop_app()
                ? Previews::SceneSpec::disabled()
                : Previews::SceneSpec{};
        }

        /// Runtime Inputs are production machine state published by plugins/runtime.
        /// Scenes only declare what they need; producers and directors own the wiring.
        /// Existing capability flags are folded into get_effective_runtime_inputs()
        /// so older scenes migrate without bespoke scheduler checks.
        [[nodiscard]] virtual SceneInputSpec get_runtime_input_spec() const { return {}; }

        [[nodiscard]] SceneInputSpec get_effective_runtime_inputs() const;

        /// Machine-readable scene capabilities used by the web app and Music
        /// Director. Preview eligibility is derived from get_preview_spec() so
        /// there is a single source of truth.
        [[nodiscard]] virtual SceneCapabilities get_capabilities() const {
            SceneCapabilities caps;
            caps.requires_desktop = const_cast<Scene *>(this)->needs_desktop_app();
            caps.can_generate_preview = get_preview_spec().enabled;
            return caps;
        }

        /// Internal preview timing hint. This is deliberately not part of
        /// SceneCapabilities: capabilities only answer whether preview_gen can
        /// generate this scene, while this controls whether it may use virtual
        /// frame time instead of waiting for wall-clock time.
        [[nodiscard]] virtual bool supports_virtual_time() const { return false; }

        /// Return true if the scene is dependent on udp packets / websocket messages from the desktop application, false if it can be rendered on the matrix directly.
        /// If this is true, the scene will only be rendered if the desktop application is running.
        [[nodiscard]] virtual bool needs_desktop_app()
        {
            return false;  // Default implementation, can be overridden
        }

        /// Set l_offscreen_canvas to nullptr if you are directly rendering onto the matrix.
        virtual void initialize(int width, int height);

        virtual void after_render_stop();

        virtual void before_transition_stop();

        [[nodiscard]] bool is_initialized() const;

        /// Returns true if the scene should continue rendering, false if not
        virtual bool render(FrameCanvas *canvas) = 0;

        /// Canonical render entrypoint. A deterministic delta is supplied by
        /// preview_gen and nested scenes; otherwise steady_clock drives dt.
        bool render_frame(FrameCanvas *canvas,
                          std::optional<double> forced_delta_seconds = std::nullopt,
                          bool suppress_internal_wait = false);

        static std::unique_ptr<Scene> from_json(const nlohmann::json &j);

        virtual void register_properties() = 0;

        virtual void load_properties(const nlohmann::json &j);

        std::vector<std::shared_ptr<Plugins::PropertyBase>> get_properties() {
            return properties;
        }
    };
}
