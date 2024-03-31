#include <atomic>
#include <optional>
#include "nlohmann/json.hpp"
#include "shared/config/MainConfig.h"

extern std::atomic<bool>  skip_image;
extern std::atomic<bool>  turned_off;
extern std::atomic<bool>  exit_canvas_update;
extern Config::MainConfig* config;