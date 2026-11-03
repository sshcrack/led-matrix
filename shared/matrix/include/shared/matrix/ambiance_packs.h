#pragma once
#include <memory>
#include <string_view>
#include <nlohmann/json.hpp>
#include <shared/matrix/config/data.h>

namespace Ambiance {
// Curated definitions are immutable. Importing creates ordinary independent
// presets, so existing editing, scheduling and activation remain authoritative.
[[nodiscard]] nlohmann::json catalog();
[[nodiscard]] std::shared_ptr<ConfigData::Preset> create_preset(std::string_view id);
}
