#include <shared/matrix/ambiance_packs.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace Ambiance {
namespace {
using json = nlohmann::json;
json look(const char *type, const char *variant, int duration) {
    return {{"type", type}, {"variant", variant}, {"arguments", {{"duration", duration}, {"weight", 1}}}};
}
const json &definitions() {
    static const json packs = json::array({
        {{"id", "quiet-evening"}, {"name", "Quiet Evening"},
         {"description", "Soft amber ribbons and sparse starlight. Long holds and gentle fades."},
         {"colors", {"#461006", "#d24f18", "#ffb24e"}}, {"transition_duration", 2500},
         {"scenes", {look("wave_pattern", "evening", 90000), look("starfield", "evening", 75000)}}},
        {{"id", "rainy-window"}, {"name", "Rainy Window"},
         {"description", "Slow blue rain, low cyan waves and a quiet, cool glow."},
         {"colors", {"#081c40", "#1c5d81", "#589eb5"}}, {"transition_duration", 2200},
         {"scenes", {look("rain", "window", 75000), look("wave_pattern", "rain_window", 90000)}}},
        {{"id", "deep-space"}, {"name", "Deep Space"},
         {"description", "Distant stars and indigo ribbons with slow motion and plenty of darkness."},
         {"colors", {"#160a3d", "#43398b", "#7ca0cd"}}, {"transition_duration", 3000},
         {"scenes", {look("starfield", "deep_space", 120000), look("wave_pattern", "deep_space", 90000)}}}
    });
    return packs;
}
std::unordered_set<std::string> scene_names() {
    std::unordered_set<std::string> names;
    for (const auto &wrapper : Plugins::PluginManager::instance()->get_scenes()) names.insert(wrapper->get_name());
    return names;
}
}

nlohmann::json catalog() {
    auto result = definitions();
    const auto names = scene_names();
    for (auto &pack : result) {
        pack["missing_scenes"] = json::array();
        for (const auto &scene : pack["scenes"])
            if (!names.contains(scene.at("type").get<std::string>())) pack["missing_scenes"].push_back(scene["type"]);
        pack["available"] = pack["missing_scenes"].empty();
    }
    return result;
}

std::shared_ptr<ConfigData::Preset> create_preset(std::string_view id) {
    const auto packs = catalog();
    const auto it = std::find_if(packs.begin(), packs.end(), [id](const auto &pack) { return pack.at("id") == id; });
    if (it == packs.end()) throw std::invalid_argument("Unknown ambiance pack");
    if (!it->at("available").get<bool>()) throw std::runtime_error("This pack requires scenes that are unavailable on this device");
    const json preset = {{"display_name", it->at("name")}, {"scenes", it->at("scenes")},
        {"transition_name", "blend"}, {"transition_duration", it->at("transition_duration")}};
    return preset.get<std::shared_ptr<ConfigData::Preset>>();
}
} // namespace Ambiance
