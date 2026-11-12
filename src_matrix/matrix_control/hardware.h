#pragma once

#include <expected>
#include <future>
#include <memory>
#include "shared/matrix/Scene.h"

class PostProcessor;
class TransitionManager;

int start_hardware_mainloop(rgb_matrix::RGBMatrixBase *matrix, PostProcessor *post_processor,
                            TransitionManager *transition_manager,
                            std::shared_ptr<Scenes::Scene> pinned_scene = nullptr);
