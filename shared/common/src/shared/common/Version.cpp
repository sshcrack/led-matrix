#include "shared/common/Version.h"
#include <regex>

namespace Common {
    
    Version Version::fromString(const std::string &versionStr) {
        Version version;
        std::regex versionRegex(R"(v?(\d+)\.(\d+)\.(\d+))");
        std::smatch matches;

        if (std::regex_search(versionStr, matches, versionRegex)) {
            version.major = std::stoi(matches[1].str());
            version.minor = std::stoi(matches[2].str());
            version.patch = std::stoi(matches[3].str());
        }

        return version;
    }

    const Version& Version::getCurrentVersion() {
        static Version currentVersion(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);
        return currentVersion;
    }

    bool Version::operator>(const Version &other) const {
        if (major != other.major)
            return major > other.major;
        if (minor != other.minor)
            return minor > other.minor;
        return patch > other.patch;
    }

    bool Version::operator==(const Version &other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    bool Version::operator<(const Version &other) const {
        return !(*this > other) && !(*this == other);
    }
    bool Version::operator<=(const Version &other) const {
        return (*this < other) || (*this == other);
    }

    bool Version::isCompatibleWith(const Version &other) const {
        return major == other.major && minor == other.minor;
    }

    std::string Version::toString() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
}