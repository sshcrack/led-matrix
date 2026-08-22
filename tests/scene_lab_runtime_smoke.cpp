#include "matrix_control/SceneLabRuntime.h"

#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/runtime_inputs.h>

#include <iostream>
#include <stdexcept>

int main() {
    RuntimeInputs::clear_all();
    auto *plugins = Plugins::PluginManager::instance();
    plugins->initialize();

    const auto wrappers = plugins->get_scenes();
    const auto it = std::find_if(wrappers.begin(), wrappers.end(), [](const auto &wrapper) {
        return wrapper && wrapper->get_name() == "starfield";
    });
    if (it == wrappers.end()) {
        std::cerr << "starfield scene is unavailable for Scene Lab smoke test\n";
        return 1;
    }

    auto &lab = SceneLabRuntime::instance();
    lab.stop();
    const auto started = lab.start("starfield", "", nlohmann::json::object(), 20, RuntimeInputs::snapshot());
    if (!started.active || !started.scene || started.session_id == 0 || started.generation == 0) {
        std::cerr << "Scene Lab did not start a generation-tracked session\n";
        return 2;
    }

    const auto updated = lab.update(
        "", started.properties, 20, RuntimeInputs::snapshot(), started.generation, started.session_id);
    if (!updated.active || updated.generation != started.generation + 1) {
        std::cerr << "Scene Lab generation did not advance after an update\n";
        return 3;
    }

    bool stale_rejected = false;
    try {
        (void)lab.update(
            "", started.properties, 20, RuntimeInputs::snapshot(), started.generation, started.session_id);
    } catch (const std::runtime_error &error) {
        stale_rejected = std::string(error.what()).find("stale") != std::string::npos;
    }
    const auto after_stale = lab.snapshot(RuntimeInputs::snapshot());
    if (!stale_rejected || after_stale.generation != updated.generation) {
        std::cerr << "stale Scene Lab update was able to replace newer state\n";
        return 4;
    }

    const auto replaced = lab.start("starfield", "", nlohmann::json::object(), 20, RuntimeInputs::snapshot());
    if (replaced.generation <= updated.generation || replaced.session_id == updated.session_id) {
        std::cerr << "replacement Scene Lab session did not advance identity/generation\n";
        return 5;
    }

    bool stale_heartbeat_rejected = false;
    try {
        lab.heartbeat(updated.session_id);
    } catch (const std::runtime_error &error) {
        stale_heartbeat_rejected = std::string(error.what()).find("stale") != std::string::npos;
    }
    if (!stale_heartbeat_rejected || !lab.lease_active(replaced.generation)) {
        std::cerr << "stale Scene Lab heartbeat affected the replacement session\n";
        return 6;
    }

    bool stale_stop_rejected = false;
    try {
        lab.stop(updated.session_id);
    } catch (const std::runtime_error &error) {
        stale_stop_rejected = std::string(error.what()).find("stale") != std::string::npos;
    }
    if (!stale_stop_rejected || !lab.snapshot(RuntimeInputs::snapshot()).active) {
        std::cerr << "stale Scene Lab stop terminated the replacement session\n";
        return 7;
    }

    lab.heartbeat(replaced.session_id);
    lab.stop(replaced.session_id);
    if (lab.snapshot(RuntimeInputs::snapshot()).active) {
        std::cerr << "Scene Lab did not stop cleanly\n";
        return 8;
    }

    plugins->delete_references();
    RuntimeInputs::clear_all();
    std::cout << "Scene Lab rejects stale generation writes\n";
    return 0;
}
