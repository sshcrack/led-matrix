#include "shared/matrix/canvas_consts.h"

namespace Constants {
    std::atomic<int> width;
    std::atomic<int> height;
    bool isRenderingSceneInitially;

    // Global post-processor instance
    PostProcessor* global_post_processor = nullptr;
}