#pragma once
// WASM override: shared/matrix/plugin/TransitionNameProperty.h
// Removes the dependency on TransitionManager / global_transition_manager.
// Transition selection is not relevant for browser-side preview rendering.

#include "shared/matrix/plugin/property.h"

namespace Plugins {

/// Sentinel value stored in config when the scene uses the preset/global default transition.
inline constexpr const char *TRANSITION_NAME_GLOBAL_DEFAULT = "__GLOBAL_DEFAULT__";

/// Simplified TransitionNameProperty for WASM builds.
/// Transition names are serialised as a string enum but the dynamic option list
/// (populated from TransitionManager at runtime on the matrix) is omitted.
class TransitionNameProperty final : public PropertyBase {
    std::string value;

public:
    explicit TransitionNameProperty()
        : PropertyBase("transition_name"), value(TRANSITION_NAME_GLOBAL_DEFAULT) {}

    [[nodiscard]] const std::string &get() const { return value; }

    void load_from_json(const nlohmann::json &j) override {
        if (j.contains(name)) {
            std::string loaded = j.at(name).get<std::string>();
            value = loaded.empty() ? TRANSITION_NAME_GLOBAL_DEFAULT : loaded;
        }
    }

    void dump_to_json(nlohmann::json &j) const override {
        j[name] = value;
    }

    void add_additional_data(nlohmann::json &j) const override {
        j["enum_name"] = "Transition";
        j["enum_values"] = nlohmann::json::array();
        j["enum_values"].push_back({{"value", TRANSITION_NAME_GLOBAL_DEFAULT},
                                    {"display_name", "Global Default"}});
        j["enum_values"].push_back({{"value", "blend"}, {"display_name", "blend"}});
    }

    [[nodiscard]] std::string get_type_id() const override { return "enum"; }
};

} // namespace Plugins
