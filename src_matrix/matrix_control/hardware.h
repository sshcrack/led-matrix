#pragma once

#include <expected>
#include <future>
#include <memory>
#include "shared/matrix/Scene.h"

int start_hardware_mainloop(rgb_matrix::RGBMatrixBase *matrix, std::shared_ptr<Scenes::Scene> pinned_scene = nullptr);