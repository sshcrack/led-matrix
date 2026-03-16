#pragma once

#include "shared/matrix/plugin/property.h"
#include "shared/matrix/canvas_consts.h"

namespace Plugins {

/// Sentinel value stored in config when the scene uses the preset/global default transition.
inline constexpr const char* TRANSITION_NAME_GLOBAL_DEFAULT = "__GLOBAL_DEFAULT__";

/// A specialised PropertyBase for the per-scene transition name.
/// It behaves like an enum property (type_id = "enum") and dynamically
/// populates its option list from the global TransitionManager at serialization
/// time, removing the need for special-case logic in scene_management.cpp.
class TransitionNameProperty final : public PropertyBase {
    std::string value;

public:
    explicit TransitionNameProperty()
        : PropertyBase("transition_name"), value(TRANSITION_NAME_GLOBAL_DEFAULT) {}

    [[nodiscard]] const std::string& get() const { return value; }

    void load_from_json(const nlohmann::json& j) override {
        if (j.contains(name)) {
            std::string loaded = j.at(name).get<std::string>();
            // Migrate legacy empty-string storage to the global-default sentinel
            value = loaded.empty() ? TRANSITION_NAME_GLOBAL_DEFAULT : loaded;
        }
    }

    void dump_to_json(nlohmann::json& j) const override {
        j[name] = value;
    }

    void add_additional_data(nlohmann::json& j) const override {
        j["enum_name"] = "Transition";
        j["enum_values"] = nlohmann::json::array();

        // First option: fall back to the preset/global default
        j["enum_values"].push_back({
            {"value", TRANSITION_NAME_GLOBAL_DEFAULT},
            {"display_name", "Global Default"}
        });

        if (Constants::global_transition_manager) {
            for (const auto& transition : Constants::global_transition_manager->get_registered_transitions()) {
                j["enum_values"].push_back({
                    {"value", transition},
                    {"display_name", transition}
                });
            }
        }

        // Fallback: ensure at least one concrete transition is always listed
        if (j["enum_values"].size() == 1) {
            j["enum_values"].push_back({
                {"value", "blend"},
                {"display_name", "blend"}
            });
        }
    }

    [[nodiscard]] std::string get_type_id() const override { return "enum"; }
};

} // namespace Plugins
