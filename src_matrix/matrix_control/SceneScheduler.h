#pragma once

#include <memory>
#include <string>
#include <vector>

#include "shared/matrix/Scene.h"
#include "shared/matrix/config/data.h"

class SceneScheduler {
public:
    struct WeightedScene {
        int weight;
        std::shared_ptr<Scenes::Scene> scene;
    };

    std::vector<WeightedScene> build_weighted_scenes(
        const std::vector<std::shared_ptr<Scenes::Scene>> &scenes,
        bool is_desktop_connected,
        std::string exclude_name = "") const;

    std::shared_ptr<Scenes::Scene> select_scene(
        const std::vector<WeightedScene> &weighted_scenes) const;

    tmillis_t resolve_transition_duration(
        const std::shared_ptr<ConfigData::Preset> &preset,
        const std::shared_ptr<Scenes::Scene> &scene) const;

    std::string resolve_transition_name(
        const std::shared_ptr<ConfigData::Preset> &preset,
        const std::shared_ptr<Scenes::Scene> &scene) const;

    bool should_schedule_transition(
        tmillis_t transition_duration,
        tmillis_t scene_duration) const;
};
