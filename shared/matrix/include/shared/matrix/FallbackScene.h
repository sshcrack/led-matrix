#include "shared/matrix/Scene.h"

namespace Scenes
{
    class FallbackScene : public Scene
    {
    private:
        std::string scene_name;

    public:
        explicit FallbackScene(const std::string &name) : scene_name(name) {}
        nlohmann::json arguments;

        int get_weight() const override {
            return -1;
        }

        tmillis_t get_duration() const override {
            return 0; // Default duration for unknown scene
        }

        int get_default_weight() override
        {
            return -1; // Default weight for unknown scene
        }

        tmillis_t get_default_duration() override
        {
            return 0; // Default duration for unknown scene
        }

        bool render(RGBMatrixBase *matrix) override
        {
            return false;
        }

        void register_properties() override
        {
        }

        std::string get_name() const override
        {
            return scene_name;
        }

        nlohmann::json to_json() const override
        {
            return arguments;
        }
    };
}