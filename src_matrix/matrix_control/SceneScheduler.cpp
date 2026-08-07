#include "SceneScheduler.h"

#include "shared/common/utils/utils.h"

using namespace std;

vector<SceneScheduler::WeightedScene>
SceneScheduler::build_weighted_scenes(
    const vector<shared_ptr<Scenes::Scene>> &scenes,
    bool is_desktop_connected,
    string exclude_name) const
{
    vector<WeightedScene> weighted_scenes;
    for (const auto &item : scenes) {
        if (item->get_weight() <= 0)
            continue;
        if (item->get_name() == exclude_name)
            continue;
        const auto capabilities = item->get_capabilities();
        if (capabilities.requires_desktop && !is_desktop_connected)
            continue;
        weighted_scenes.emplace_back(item->get_weight(), item);
    }
    return weighted_scenes;
}

shared_ptr<Scenes::Scene>
SceneScheduler::select_scene(
    const vector<WeightedScene> &weighted_scenes) const
{
    if (weighted_scenes.empty())
        return nullptr;

    int total_weight = 0;
    for (const auto &[weight, _] : weighted_scenes)
        total_weight += weight;

    const auto selected = get_random_number_inclusive(0, total_weight);
    int curr_weight = 0;
    for (const auto &[weight, curr_scene] : weighted_scenes) {
        curr_weight += weight;
        if (curr_weight >= selected)
            return curr_scene;
    }

    return weighted_scenes.front().scene;
}

tmillis_t
SceneScheduler::resolve_transition_duration(
    const shared_ptr<ConfigData::Preset> &preset,
    const shared_ptr<Scenes::Scene> &scene) const
{
    const auto scene_override = scene->get_transition_duration();
    if (scene_override > 0)
        return scene_override;
    return preset->transition_duration;
}

string
SceneScheduler::resolve_transition_name(
    const shared_ptr<ConfigData::Preset> &preset,
    const shared_ptr<Scenes::Scene> &scene) const
{
    const auto scene_override = scene->get_transition_name();
    if (!scene_override.empty() && scene_override != Plugins::TRANSITION_NAME_GLOBAL_DEFAULT)
        return scene_override;
    return preset->transition_name;
}

bool
SceneScheduler::should_schedule_transition(
    tmillis_t transition_duration,
    tmillis_t scene_duration) const
{
    return transition_duration > 0 && transition_duration < scene_duration;
}
