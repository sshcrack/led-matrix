#pragma once

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <shared/common/utils/utils.h>

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
    fs::path exeDir = get_exec_dir();

    fs::path portableFile = exeDir / "portable.txt";
    bool isPortable = fs::exists(portableFile);

    if (isPortable)
        return exeDir.parent_path() / "data";

    fs::path localConfig = exeDir / "config.json";
    if (isWritableExistingFile(localConfig))
        return localConfig.string();

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
    return configRootPath / "config.json";
#else
    const char *home = getenv("HOME");
    if (!home)
        throw std::runtime_error("Failed to get HOME environment variable");

    fs::path configRootPath = fs::path(home) / ".config" / "led-matrix-desktop";
    if (!fs::exists(configRootPath))
        fs::create_directories(configRootPath);

    return configRootPath / "config.json";
#endif
}