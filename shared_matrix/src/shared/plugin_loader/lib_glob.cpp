#include "lib_glob.h"
#include <spdlog/spdlog.h>

// based on https://stackoverflow.com/a/8615450/4765406
std::vector<std::string> Plugins::lib_glob(const std::string &pattern) {
    glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));

    spdlog::debug("Globbing for {}", pattern);
    int glob_ret = glob(pattern.c_str(), GLOB_TILDE, nullptr, &glob_result);
    if (glob_ret != 0) {
        globfree(&glob_result);

        if(glob_ret != GLOB_NOMATCH)
            spdlog::error("glob failed, return value {}", glob_ret);
        else
            spdlog::warn("Glob could not find any plugins.");
        // Ignoring exceptions
        return std::vector<std::string>(0);
    }

    std::vector<std::string> filenames;
    for (size_t i = 0; i < glob_result.gl_pathc; i++) {
        filenames.emplace_back(glob_result.gl_pathv[i]);
    }

    globfree(&glob_result);

    return filenames;
}
