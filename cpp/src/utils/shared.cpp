#include "shared.h"
#include "../config/MainConfig.h"

std::atomic<bool> skip_image = false;
std::atomic<bool> exit_canvas_update = false;
std::atomic<bool> turned_off = false;
Config::MainConfig* config = nullptr;