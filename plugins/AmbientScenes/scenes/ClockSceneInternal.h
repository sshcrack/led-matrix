// Private helpers shared by the ClockScene translation units.
#pragma once

#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include "ClockScene.h"
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace {
inline std::tm* compat_localtime_r(const std::time_t* t, std::tm* out) {
#ifdef _WIN32
    return localtime_s(out, t) == 0 ? out : nullptr;
#else
    return localtime_r(t, out);
#endif
}
}
