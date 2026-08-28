#pragma once

#include <atomic>
#include "nlohmann/json.hpp"
#include "shared/matrix/config/MainConfig.h"
#include "shared/matrix/export.h"

extern SHARED_MATRIX_API std::atomic<bool> skip_image;
extern SHARED_MATRIX_API std::atomic<bool> exit_canvas_update;
extern SHARED_MATRIX_API Config::MainConfig* config;
