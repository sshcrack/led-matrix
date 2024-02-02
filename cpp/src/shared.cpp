#include "shared.h"
#include "Config.h"

std::atomic<bool> skip_image = false;
std::atomic<bool> exit_canvas_update = false;
Config* config = nullptr;