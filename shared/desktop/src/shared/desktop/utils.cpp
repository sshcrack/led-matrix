#include "shared/desktop/utils.h"
#include <fstream>

namespace fs = std::filesystem;
bool isWritableExistingFile(const std::filesystem::path &path)
{
    if (!std::filesystem::exists(path))
        return false;

    std::ofstream ofs(path, std::ios::in | std::ios::out); // read/write mode
    return ofs.is_open();
}

fs::path get_data_dir()
{
    fs::path configPath;
    const fs::path exeDir = get_exec_dir();

    const fs::path portableFile = exeDir / "portable.txt";
    bool isPortable = fs::exists(portableFile);

    auto localDataDir = exeDir.parent_path() / "data";
    if (isPortable)
        return localDataDir;

    const fs::path dataConfigJson = localDataDir / "config.json";
    if (isWritableExistingFile(dataConfigJson))
        return localDataDir;

#ifdef _WIN32
    char *appData = nullptr;
    size_t sz = 0;
    _dupenv_s(&appData, &sz, "APPDATA");
    if (!appData)
        throw std::runtime_error("Failed to get APPDATA environment variable");

    fs::path configRootPath = fs::path(appData) / "led-matrix-desktop";
    if (!fs::exists(configRootPath))
        fs::create_directories(configRootPath);

    free(appData);
    return configRootPath;
#else
    const char *home = getenv("HOME");
    if (!home)
        throw std::runtime_error("Failed to get HOME environment variable");

    fs::path configRootPath = fs::path(home) / ".config" / "led-matrix-desktop";
    if (!fs::exists(configRootPath))
        fs::create_directories(configRootPath);

    return configRootPath;
#endif
}