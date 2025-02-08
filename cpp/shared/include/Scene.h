#pragma once

#include "shared/utils/utils.h"
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include "led-matrix.h"
#include "content-streamer.h"
#include "plugin/property.h"
#include <vector>

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using std::string;

namespace Scenes {
    class Scene {
    private:
        bool create_offscreen = false;
        std::vector<std::pair<string, PropertyBase*>> properties;

        template<typename T>
        void add_property_single(Property<T> *property) {
            std::string name = property->getName();
            for (const auto &item: properties) {
                if (item.second == property) {
                    return;
                }

                if (item.first == name) {
                    throw std::runtime_error(fmt::format("Property with name '{}' already exists", name));
                }
            }

            properties.emplace_back(name, property);
        }

    protected:
        bool initialized = false;
        /// Only available after initialization
        int matrix_width;
        /// Only available after initialization
        int matrix_height;

        rgb_matrix::FrameCanvas *offscreen_canvas = nullptr;

        Property<int> weight = Property("weight", 1, true);
        Property<tmillis_t> duration = Property("duration", static_cast<tmillis_t>(0), true);

    public:
        explicit Scene(bool create_offscreen = true);

        [[nodiscard]] virtual int get_weight() const;

        [[nodiscard]] virtual tmillis_t get_duration() const;

        [[nodiscard]] virtual nlohmann::json to_json() const;

        [[nodiscard]] virtual string get_name() const = 0;

        virtual void initialize(rgb_matrix::RGBMatrix *matrix);

        virtual void after_render_stop(rgb_matrix::RGBMatrix *matrix);

        [[nodiscard]] bool is_initialized() const;

        /// Returns true if the scene should continue rendering, false if not
        virtual bool render(rgb_matrix::RGBMatrix *matrix) = 0;

        static Scene *from_json(const nlohmann::json &j);

        virtual void register_properties() = 0;

        virtual void load_properties(const nlohmann::json &j);

        template<typename T>
        void add_property(Property<T> *property) {
            add_property_single(property);
        }

        template<typename T, typename... Args>
        void add_property(Property<T> *t, Args... args) // recursive variadic function
        {
            add_property_single(t);

            add_property(args...);
        }
    };
}