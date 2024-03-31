//
// Created by sshcrack on 31.03.24.
//

#pragma once
#include <thread>

namespace PluginLoader {
    std::pair<std::thread, std::thread> initialize();
}