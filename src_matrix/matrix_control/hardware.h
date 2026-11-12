#pragma once

#include <expected>
#include <future>
#include <memory>
#include "shared/matrix/Scene.h"

namespace Config {
class MainConfig;
}
class PostProcessor;
class TransitionManager;

int start_hardware_mainloop(rgb_matrix::RGBMatrixBase* matrix, PostProcessor* post_processor, TransitionManager* transition_manager,
                            Config::MainConfig* cfg, std::shared_ptr<Scenes::Scene> pinned_scene = nullptr);
