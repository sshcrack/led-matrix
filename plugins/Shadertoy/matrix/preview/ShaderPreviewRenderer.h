#pragma once

#include <filesystem>
#include <string_view>

#include "led-matrix.h"
#include "shared/matrix/preview.h"
#include "shared/matrix/scene_runtime.h"

namespace ShadertoyPreview {

inline constexpr std::string_view InputId = "shadertoy.render";

/// Store the current preview-generator configuration. Called by the plugin's
/// preview DataProvider before a Shadertoy scene begins rendering.
void begin(const Previews::RunContext& context);
void end() noexcept;

/// Render one deterministic frame of a local shader for preview_gen. GPU work is
/// performed by an isolated helper process so it cannot collide with the
/// emulator's graphics state.
bool render_shader(const std::filesystem::path& shader_path, rgb_matrix::FrameCanvas* canvas, const Scenes::SceneFrameContext& frame);

}  // namespace ShadertoyPreview
