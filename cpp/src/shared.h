#include <atomic>
#include <optional>
#include "nlohmann/json.hpp"
#include "Config.h"

extern std::atomic<bool>  skip_image;
extern Config* config;