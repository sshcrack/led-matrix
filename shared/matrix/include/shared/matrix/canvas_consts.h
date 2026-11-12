#pragma once

#include <atomic>
#include <string>

#include "shared/matrix/export.h"

namespace Constants {
    extern SHARED_MATRIX_API std::atomic<int> width;
    extern SHARED_MATRIX_API std::atomic<int> height;
}
