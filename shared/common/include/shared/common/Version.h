#pragma once

#include <string>
#include "shared/common/macro.h"

namespace Common {
    struct SHARED_COMMON_API Version {
        int major = 0;
        int minor = 0;
        int patch = 0;

        Version() = default;
        Version(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}

        // Parse version from string like "v1.2.3" or "1.2.3"
        static Version fromString(const std::string &versionStr);

        // Compare versions
        bool operator>(const Version &other) const;
        bool operator==(const Version &other) const;
        bool operator<(const Version &other) const;

        std::string toString() const;
    };
}