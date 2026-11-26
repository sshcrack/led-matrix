#pragma once

#include <shared/matrix/audio_kit.h>
#include <shared/matrix/utils/AmbiancePalette.h>

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"

namespace Scenes {
    class WatermelonPlasmaScene : public Scene {
        PropertyPointer<Plugins::EnumProperty<Ambiance::Palette>> look_palette_ =
            MAKE_ENUM_PROPERTY("look_palette", Ambiance::Palette, Ambiance::Palette::ORIGINAL);
        PropertyPointer<float> look_brightness_ = MAKE_PROPERTY_MINMAX("look_brightness", float, 1.0f, 0.05f, 1.0f);
        PropertyPointer<float> speed_ = MAKE_PROPERTY_MINMAX("speed", float, 1.0f, 0.2f, 2.5f);

        AudioKit::Buffer buf_;
        float time_ = 0.0f;

    public:
        ~WatermelonPlasmaScene() override = default;
        void initialize(int width, int height) override;
        bool render(rgb_matrix::FrameCanvas *canvas) override;
        void register_properties() override;

        [[nodiscard]] string get_name() const override;
        [[nodiscard]] std::string get_category() const override { return "Generative"; }
        [[nodiscard]] SceneDescriptor get_descriptor() const override;
        [[nodiscard]] bool supports_virtual_time() const override { return true; }

        using Scenes::Scene::Scene;

        tmillis_t get_default_duration() override { return 10000; }
        int get_default_weight() override { return 1; }
    };

    class WatermelonPlasmaSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene> create() override;
    };
    }  // namespace Scenes
