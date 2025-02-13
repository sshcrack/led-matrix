#pragma once

#include "shared/utils/utils.h"
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include "led-matrix.h"
#include "content-streamer.h"
#include "plugin/property.h"
#include <vector>
#include "shared/utils/PropertyMacros.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using std::string;

namespace Scenes {
    class Scene {
    private:
        std::vector<std::shared_ptr<PropertyBase>> properties;

    protected:
        bool initialized = false;
        int matrix_width;
        int matrix_height;

        PropertyPointer<int> weight = MAKE_PROPERTY_REQ("weight", int, 1);
        PropertyPointer<tmillis_t> duration = MAKE_PROPERTY("duration", tmillis_t, 0);

    public:
        rgb_matrix::FrameCanvas *offscreen_canvas = nullptr;

        Scene();

        virtual ~Scene() = default;  // Changed to proper virtual destructor with default implementation

        void add_property(std::shared_ptr<PropertyBase> property) {
            std::string name = property->getName();
            for (const auto &item: properties) {
                if (item->getName() == name) {
                    throw std::runtime_error(fmt::format("Property with name '{}' already exists", name));
                }
            }

            properties.push_back(property);
        }

        [[nodiscard]] virtual int get_weight() const;

        [[nodiscard]] virtual tmillis_t get_duration() const;

        [[nodiscard]] virtual nlohmann::json to_json() const;

        [[nodiscard]] virtual string get_name() const = 0;

        virtual void initialize(rgb_matrix::RGBMatrix *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas);

        virtual void after_render_stop(rgb_matrix::RGBMatrix *matrix);

        [[nodiscard]] bool is_initialized() const;

        /// Returns true if the scene should continue rendering, false if not
        virtual bool render(rgb_matrix::RGBMatrix *matrix) = 0;

        static std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> from_json(const nlohmann::json &j);

        virtual void register_properties() = 0;

        virtual void load_properties(const nlohmann::json &j);

        std::vector<std::shared_ptr<PropertyBase>> get_properties() {
            return properties;
        }
    };
}