#pragma once

#include "shared/common/macro.h"

namespace Common {
/// Fingerprint of the scene code (plugins/ + shared/) this binary was built
/// from; see cmake/SceneCodeDigest.cmake. "unknown" outside a git checkout.
/// Two builds render identical scenes only if their fingerprints match.
SHARED_COMMON_API const char *scene_code_digest();
}
