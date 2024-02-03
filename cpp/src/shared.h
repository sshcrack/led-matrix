#include <atomic>
#include <optional>
#include "nlohmann/json.hpp"
#include "config/MainConfig.h"

extern std::atomic<bool>  skip_image;
extern std::atomic<bool>  exit_canvas_update;
extern Config::MainConfig* config;