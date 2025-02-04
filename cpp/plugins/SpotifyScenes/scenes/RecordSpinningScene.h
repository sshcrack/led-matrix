#pragma once

#include <optional>
#include "Scene.h"
#include "shared/utils/utils.h"
#include "../manager/state.h"
#include "wrappers.h"

namespace Scenes {


    class RecordSpinningScene : public Scene {
    public:
        bool render(rgb_matrix::RGBMatrix *matrix) override;
        [[nodiscard]] int get_weight() const override;
        [[nodiscard]] string get_name() const override;

        using Scene::Scene;
    };

    class RecordSpinningSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create_default() override;

        Scenes::Scene *from_json(const nlohmann::json &args) override;
    };
}
