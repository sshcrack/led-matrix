#pragma once

#include "shared/utils/utils.h"
#include "nlohmann/json.hpp"
#include "fmt/format.h"
#include "led-matrix.h"
#include "content-streamer.h"
#include "plugin/property.h"
#include <vector>
#include "shared/utils/PropertyMacros.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::RGBMatrixBase;
using std::string;

namespace Scenes {
    class Scene {
        std::vector<std::shared_ptr<Plugins::PropertyBase>> properties;

    protected:
        bool initialized = false;
        int matrix_width;
        int matrix_height;
        int target_fps = 60;
        tmillis_t last_render_time = 0;

        // Pure virtual methods remain unchanged
        virtual int get_default_weight() = 0;
        virtual tmillis_t get_default_duration() = 0;

        // Initialize with temporary values instead of calling virtual functions
        PropertyPointer<int> weight = MAKE_PROPERTY("weight", int, 1);
        PropertyPointer<tmillis_t> duration = MAKE_PROPERTY("duration", tmillis_t, 5000);

        std::string uuid;

        void set_target_fps(int fps) {
            target_fps = fps;
        }

        [[nodiscard]] int get_target_fps() const {
            return target_fps;
        }

        virtual bool should_render_frame();
        virtual void wait_until_next_frame();

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
        FrameCanvas *offscreen_canvas = nullptr;

        Scene();

        virtual ~Scene() = default;  // Changed to proper virtual destructor with default implementation

        [[nodiscard]] std::string get_uuid() const {
            return this->uuid;
        }


        /// This method is used to update the default of properties dynamically. It is called before a property has been registered.
        virtual void update_default_properties() {
            weight->set_value(get_default_weight());
            duration->set_value(get_default_duration());
        }

        [[nodiscard]] virtual int get_weight() const;

        [[nodiscard]] virtual tmillis_t get_duration() const;

        [[nodiscard]] virtual nlohmann::json to_json() const;

        [[nodiscard]] virtual string get_name() const = 0;

        virtual void initialize(RGBMatrixBase *matrix, FrameCanvas *l_offscreen_canvas);

        virtual void after_render_stop(RGBMatrixBase *matrix);

        [[nodiscard]] bool is_initialized() const;

        /// Returns true if the scene should continue rendering, false if not
        virtual bool render(RGBMatrixBase *matrix) = 0;

        static std::unique_ptr<Scene, void (*)(Scene *)> from_json(const nlohmann::json &j);

        virtual void register_properties() = 0;

        virtual void load_properties(const nlohmann::json &j);

        std::vector<std::shared_ptr<Plugins::PropertyBase>> get_properties() {
            return properties;
        }
    };
}