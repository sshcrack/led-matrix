#ifdef _WIN32
#include "shared/common/win_compat.h"
#endif
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/plugin_registry.h"
#include <shared/matrix/input_ids.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <regex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

using namespace spdlog;
using Plugins::BasicPlugin;
using Plugins::ImageProviderWrapper;
using Plugins::PluginManager;
using Plugins::RegistryValidationReport;
using Plugins::SceneWrapper;

namespace {
bool has_control_or_path_separator(const std::string &value) {
    for (unsigned char c : value) {
        if (c < 0x20 || c == '/' || c == '\\') return true;
    }
    return false;
}

bool snake_caseish(const std::string &value) {
    if (value.empty() || !std::islower(static_cast<unsigned char>(value.front()))) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::islower(c) || std::isdigit(c) || c == '_';
    });
}
}

PluginManager::PluginManager()
#ifdef _WIN32
    : PluginLoader("led-matrix")
#else
    : PluginLoader("led-matrix", RTLD_LAZY | RTLD_GLOBAL)
#endif
{}

PluginManager *PluginManager::instance_ = nullptr;

PluginManager *PluginManager::instance() {
    if (instance_ == nullptr) instance_ = new PluginManager();
    return instance_;
}

std::vector<BasicPlugin *> PluginManager::get_plugins() {
    std::vector<BasicPlugin *> plugins;
    plugins.reserve(loaded_plugins.size());
    for (const auto &item : loaded_plugins) plugins.emplace_back(item.plugin);
    return plugins;
}

std::vector<std::shared_ptr<SceneWrapper>> PluginManager::get_scenes() {
    std::lock_guard<std::mutex> lock(scenes_mutex);
    const auto plugins = get_plugins();
    // get_scenes() can be queried while configuration is being constructed,
    // before PluginLoader::initialize() has discovered any DSOs. Do not let
    // that empty early query permanently poison the scene cache. Rebuild when
    // the loaded plugin set changes (notably 0 -> N during preview_gen startup).
    if (!scenes_initialized || scenes_plugin_count_ != plugins.size()) {
        all_scenes.clear();
        for (auto *item : plugins) {
            auto pl_scenes = item->get_scenes();
            all_scenes.insert(all_scenes.end(), pl_scenes.begin(), pl_scenes.end());
        }
        scenes_initialized = true;
        scenes_plugin_count_ = plugins.size();
        validation_report_ = validate_registry(false);
        if (!validation_report_.ok()) {
            for (const auto &error : validation_report_.errors)
                spdlog::error("Registry validation: {}", error);
            throw std::runtime_error(fmt::format(
                "Plugin/scene registry validation failed with {} error(s)",
                validation_report_.errors.size()));
        }
        for (const auto &warning : validation_report_.warnings)
            spdlog::warn("Registry validation: {}", warning);
    }
    return all_scenes;
}

void PluginManager::add_scene(std::shared_ptr<SceneWrapper> scene) {
    if (!scene) throw std::invalid_argument("Cannot register a null scene wrapper");
    std::lock_guard<std::mutex> lock(scenes_mutex);
    const auto name = scene->get_name();
    const auto duplicate = std::find_if(all_scenes.begin(), all_scenes.end(), [&](const auto &existing) {
        return existing && existing->get_name() == name;
    });
    if (duplicate != all_scenes.end())
        throw std::runtime_error(fmt::format("Duplicate scene id '{}'", name));
    all_scenes.push_back(std::move(scene));
    const auto report = validate_registry(false);
    if (!report.ok()) {
        all_scenes.pop_back();
        validation_report_ = validate_registry(false);
        throw std::runtime_error(fmt::format(
            "Dynamic scene '{}' failed registry validation: {}",
            name, report.errors.empty() ? "unknown error" : report.errors.front()));
    }
    validation_report_ = report;
}

void PluginManager::remove_scene(const std::string& name) {
    std::lock_guard<std::mutex> lock(scenes_mutex);
    all_scenes.erase(
        std::remove_if(all_scenes.begin(), all_scenes.end(),
                       [&name](const std::shared_ptr<SceneWrapper>& s) {
                           return s && s->get_name() == name;
                       }),
        all_scenes.end());
    validation_report_ = validate_registry(false);
}

std::vector<std::shared_ptr<ImageProviderWrapper>> PluginManager::get_image_providers() {
    std::vector<std::shared_ptr<ImageProviderWrapper>> types;
    for (const auto &item : get_plugins()) {
        auto pl_providers = item->get_image_providers();
        types.insert(types.end(), pl_providers.begin(), pl_providers.end());
    }
    return types;
}

std::vector<std::shared_ptr<Plugins::ShaderProviderWrapper>> PluginManager::get_shader_providers() {
    std::vector<std::shared_ptr<Plugins::ShaderProviderWrapper>> types;
    for (const auto &item : get_plugins()) {
        auto pl_providers = item->get_shader_providers();
        types.insert(types.end(), pl_providers.begin(), pl_providers.end());
    }
    return types;
}

RegistryValidationReport PluginManager::validate_registry(bool throw_on_error) {
    RegistryValidationReport report;

    std::unordered_map<std::string, std::string> plugins_by_name;
    for (const auto &item : loaded_plugins) {
        if (!item.plugin) {
            report.errors.emplace_back(fmt::format("Loaded plugin '{}' has a null instance", item.name));
            continue;
        }
        const auto plugin_name = item.plugin->get_plugin_name();
        if (plugin_name.empty()) {
            report.errors.emplace_back(fmt::format("Plugin library '{}' reports an empty plugin name", item.name));
            continue;
        }
        if (auto [it, inserted] = plugins_by_name.emplace(plugin_name, item.name); !inserted) {
            report.errors.emplace_back(fmt::format(
                "Duplicate plugin name '{}' from '{}' and '{}'", plugin_name, it->second, item.name));
        }
    }

    std::unordered_map<std::string, std::string> runtime_input_owners{
        {std::string(RuntimeInputIds::Desktop), "matrix runtime"}
    };
    for (const auto &item : loaded_plugins) {
        if (!item.plugin) continue;
        for (const auto &input_id : item.plugin->get_runtime_input_ids()) {
            if (input_id.empty()) {
                report.errors.emplace_back(fmt::format(
                    "Plugin '{}' declares an empty Runtime Input id", item.plugin->get_plugin_name()));
                continue;
            }
            if (has_control_or_path_separator(input_id)) {
                report.errors.emplace_back(fmt::format(
                    "Plugin '{}' declares invalid Runtime Input id '{}'",
                    item.plugin->get_plugin_name(), input_id));
                continue;
            }
            if (auto [it, inserted] = runtime_input_owners.emplace(input_id, item.plugin->get_plugin_name()); !inserted) {
                report.errors.emplace_back(fmt::format(
                    "Duplicate Runtime Input producer '{}' from '{}' and '{}'",
                    input_id, it->second, item.plugin->get_plugin_name()));
            }
        }
    }

    std::unordered_map<std::string, std::string> preview_provider_owners;
    for (const auto &item : loaded_plugins) {
        if (!item.plugin) continue;
        for (const auto &provider : item.plugin->get_preview_data_providers()) {
            if (!provider) {
                report.errors.emplace_back(fmt::format(
                    "Plugin '{}' contains a null preview data provider", item.plugin->get_plugin_name()));
                continue;
            }
            const std::string provider_id(provider->id());
            if (provider_id.empty()) {
                report.errors.emplace_back(fmt::format(
                    "Plugin '{}' contains a preview data provider with an empty id", item.plugin->get_plugin_name()));
                continue;
            }
            if (auto [it, inserted] = preview_provider_owners.emplace(provider_id, item.plugin->get_plugin_name()); !inserted) {
                report.errors.emplace_back(fmt::format(
                    "Duplicate preview data provider '{}' from plugins '{}' and '{}'",
                    provider_id, it->second, item.plugin->get_plugin_name()));
            }
        }
    }

    std::unordered_map<std::string, size_t> scene_names;
    for (size_t index = 0; index < all_scenes.size(); ++index) {
        const auto &wrapper = all_scenes[index];
        if (!wrapper) {
            report.errors.emplace_back(fmt::format("Scene wrapper #{} is null", index));
            continue;
        }

        std::string scene_name;
        std::shared_ptr<Scenes::Scene> scene;
        try {
            scene_name = wrapper->get_name();
            scene = wrapper->get_default();
        } catch (const std::exception &e) {
            report.errors.emplace_back(fmt::format("Scene wrapper #{} failed to initialize: {}", index, e.what()));
            continue;
        }

        if (scene_name.empty())
            report.errors.emplace_back(fmt::format("Scene wrapper #{} has an empty scene id", index));
        if (has_control_or_path_separator(scene_name))
            report.errors.emplace_back(fmt::format("Scene id '{}' contains a control/path character", scene_name));
        if (auto [it, inserted] = scene_names.emplace(scene_name, index); !inserted)
            report.errors.emplace_back(fmt::format("Duplicate scene id '{}' (wrappers #{} and #{})", scene_name, it->second, index));

        if (!scene) {
            report.errors.emplace_back(fmt::format("Scene '{}' returned a null default scene", scene_name));
            continue;
        }

        const auto caps = scene->get_capabilities();
        if (caps.requires_audio && !caps.requires_desktop)
            report.warnings.emplace_back(fmt::format("Scene '{}' requires audio but does not declare requires_desktop", scene_name));

        const auto runtime_spec = scene->get_effective_runtime_inputs();
        std::unordered_set<std::string> runtime_input_ids;
        for (const auto &input : runtime_spec.required) {
            if (input.empty()) {
                report.errors.emplace_back(fmt::format("Scene '{}' requires an empty Runtime Input id", scene_name));
                continue;
            }
            if (!runtime_input_ids.insert(input).second)
                report.errors.emplace_back(fmt::format("Scene '{}' declares Runtime Input '{}' more than once", scene_name, input));
            if (!input.empty() && !runtime_input_owners.contains(input))
                report.errors.emplace_back(fmt::format(
                    "Scene '{}' requires Runtime Input '{}' but no producer declares it", scene_name, input));
        }
        for (const auto &input : runtime_spec.optional) {
            if (input.empty()) {
                report.errors.emplace_back(fmt::format("Scene '{}' accepts an empty Runtime Input id", scene_name));
                continue;
            }
            if (!runtime_input_ids.insert(input).second)
                report.errors.emplace_back(fmt::format(
                    "Scene '{}' declares Runtime Input '{}' as both required and optional",
                    scene_name, input));
            if (!input.empty() && !runtime_input_owners.contains(input))
                report.errors.emplace_back(fmt::format(
                    "Scene '{}' accepts Runtime Input '{}' but no producer declares it", scene_name, input));
        }

        const auto preview_spec = scene->get_preview_spec();
        if (preview_spec.enabled) {
            std::unordered_set<std::string> requested_inputs;
            for (const auto &input : preview_spec.inputs) {
                if (input.empty()) {
                    report.errors.emplace_back(fmt::format("Scene '{}' requests an empty preview input id", scene_name));
                    continue;
                }
                if (!requested_inputs.insert(input).second)
                    report.errors.emplace_back(fmt::format("Scene '{}' requests preview input '{}' more than once", scene_name, input));
                if (!preview_provider_owners.contains(input))
                    report.errors.emplace_back(fmt::format(
                        "Scene '{}' requests preview input '{}' but no plugin provides it", scene_name, input));
            }
        }

        const auto properties = scene->get_properties();
        std::unordered_set<std::string> property_names;
        std::unordered_set<std::string> property_input_names;
        for (const auto &property : properties) {
            if (!property) {
                report.errors.emplace_back(fmt::format("Scene '{}' contains a null property", scene_name));
                continue;
            }
            const auto &property_name = property->getName();
            if (!property_names.insert(property_name).second)
                report.errors.emplace_back(fmt::format("Scene '{}' registers property '{}' more than once", scene_name, property_name));
            if (!property_input_names.insert(property_name).second)
                report.errors.emplace_back(fmt::format("Scene '{}' property input name '{}' is ambiguous", scene_name, property_name));
            for (const auto &legacy_name : property->legacy_names())
                if (!property_input_names.insert(legacy_name).second)
                    report.errors.emplace_back(fmt::format("Scene '{}' legacy property input name '{}' is ambiguous", scene_name, legacy_name));
            if (!snake_caseish(property_name))
                report.warnings.emplace_back(fmt::format("Scene '{}' property '{}' is not snake_case", scene_name, property_name));
            for (const auto &issue : property->validate_schema())
                report.errors.emplace_back(fmt::format("Scene '{}' property '{}': {}", scene_name, property_name, issue));
        }
        const auto descriptor = scene->get_descriptor();
        const auto score_valid = [](float value) {
            return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
        };
        if (descriptor.family.empty())
            report.errors.emplace_back(fmt::format("Scene '{}' descriptor has an empty family", scene_name));
        for (const auto [label, value] : std::array<std::pair<std::string_view, float>, 4>{
                 {{"intensity", descriptor.intensity}, {"motion", descriptor.motion},
                  {"music_affinity", descriptor.music_affinity}, {"performance_cost", descriptor.performance_cost}}}) {
            if (!score_valid(value))
                report.errors.emplace_back(fmt::format(
                    "Scene '{}' descriptor {} must be in the range 0..1", scene_name, label));
        }
        std::unordered_set<std::string> descriptor_tags;
        for (const auto &tag : descriptor.tags) {
            if (tag.empty())
                report.errors.emplace_back(fmt::format("Scene '{}' descriptor contains an empty tag", scene_name));
            else if (!descriptor_tags.insert(tag).second)
                report.errors.emplace_back(fmt::format("Scene '{}' descriptor repeats tag '{}'", scene_name, tag));
        }
        std::unordered_set<std::string> variant_ids;
        for (const auto &variant : descriptor.variants) {
            if (variant.id.empty() || !snake_caseish(variant.id))
                report.errors.emplace_back(fmt::format(
                    "Scene '{}' variant id '{}' must be non-empty snake_case", scene_name, variant.id));
            if (!variant_ids.insert(variant.id).second)
                report.errors.emplace_back(fmt::format("Scene '{}' repeats variant id '{}'", scene_name, variant.id));
            if (variant.label.empty())
                report.errors.emplace_back(fmt::format("Scene '{}' variant '{}' has no label", scene_name, variant.id));
            if (!variant.properties.is_object())
                report.errors.emplace_back(fmt::format("Scene '{}' variant '{}' properties must be an object", scene_name, variant.id));
            else
                for (const auto &[property_name, _] : variant.properties.items())
                    if (!property_names.contains(property_name))
                        report.errors.emplace_back(fmt::format(
                            "Scene '{}' variant '{}' overrides unknown property '{}'",
                            scene_name, variant.id, property_name));
            for (const auto &[label, value] : std::array<std::pair<std::string_view, std::optional<float>>, 4>{
                     {{"intensity", variant.intensity}, {"motion", variant.motion},
                      {"music_affinity", variant.music_affinity}, {"performance_cost", variant.performance_cost}}}) {
                if (value.has_value() && !score_valid(*value))
                    report.errors.emplace_back(fmt::format(
                        "Scene '{}' variant '{}' {} must be in the range 0..1",
                        scene_name, variant.id, label));
            }
            if (!variant.id.empty() && variant.properties.is_object()) {
                try {
                    auto configured = wrapper->create();
                    configured->update_default_properties();
                    configured->register_properties();
                    configured->apply_variant(variant.id);
                } catch (const std::exception &e) {
                    report.errors.emplace_back(fmt::format(
                        "Scene '{}' variant '{}' cannot be applied: {}", scene_name, variant.id, e.what()));
                }
            }
        }

        if (preview_spec.enabled && preview_spec.property_overrides.is_object()) {
            for (const auto &[property_name, _] : preview_spec.property_overrides.items()) {
                if (!property_names.contains(property_name))
                    report.errors.emplace_back(fmt::format(
                        "Scene '{}' preview overrides unknown property '{}'", scene_name, property_name));
            }
        }
        for (const auto &property : properties) {
            if (!property) continue;
            const auto &dependency = property->ui_metadata().visible_if_property;
            if (!dependency.empty() && !property_names.contains(dependency))
                report.errors.emplace_back(fmt::format(
                    "Scene '{}' property '{}' visibility depends on unknown property '{}'",
                    scene_name, property->getName(), dependency));
        }
    }

    validation_report_ = report;
    if (throw_on_error && !report.ok())
        throw std::runtime_error(fmt::format("Registry validation failed with {} error(s)", report.errors.size()));
    return report;
}

void PluginManager::delete_references() {
    std::lock_guard<std::mutex> lock(scenes_mutex);
    all_scenes.clear();
    scenes_initialized = false;
    scenes_plugin_count_ = 0;
    validation_report_ = {};

    // std::any type-erasure managers can be emitted into the plugin DSO that
    // stored the value. Destroy registry values before PluginLoader dlcloses
    // those DSOs, otherwise process teardown may call code that is unloaded.
    PluginRegistry::clear();
}
