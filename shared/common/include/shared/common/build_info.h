#pragma once

#include "shared/common/macro.h"

namespace Common {
/// Versioned SHA-256 of portable scene sources, resources, worker and build
/// inputs; see cmake/SceneCodeDigest.cmake. Independent of Git and build flavor.
/// Matching values identify compatible source inputs, not pixel-identical output
/// across compilers/architectures. Runtime scene availability is checked separately.
SHARED_COMMON_API const char *scene_code_digest();
}
