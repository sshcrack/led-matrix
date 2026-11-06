#include "matrix_control/SceneLabRuntime.h"

#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/runtime_inputs.h>

#include <chrono>
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

    // Lease expiry: an Active session whose lease elapsed behaves as Inactive
    // for every caller, and the owner learns it through StaleSession (409).
    auto fake_now = std::chrono::steady_clock::now();
    lab.set_clock_for_testing([&fake_now] { return fake_now; });
    const auto leased = lab.start("starfield", "", nlohmann::json::object(), 20, RuntimeInputs::snapshot());
    fake_now += SceneLabRuntime::LeaseDuration + std::chrono::seconds(1);
    if (lab.snapshot().active || lab.lease_active(leased.generation)) {
        std::cerr << "expired Scene Lab lease still reported active\n";
        return 9;
    }
    bool expired_update_rejected = false;
    try {
        (void)lab.update("", leased.properties, 20, RuntimeInputs::snapshot(), leased.generation, leased.session_id);
    } catch (const SceneLabRuntime::StaleSession &) {
        expired_update_rejected = true;
    }
    if (!expired_update_rejected || lab.snapshot().active) {
        std::cerr << "update revived an expired Scene Lab session\n";
        return 10;
    }
    bool expired_heartbeat_rejected = false;
    try {
        lab.heartbeat(leased.session_id);
    } catch (const SceneLabRuntime::StaleSession &) {
        expired_heartbeat_rejected = true;
    }
    if (!expired_heartbeat_rejected) {
        std::cerr << "heartbeat for an expired Scene Lab session did not report it stale\n";
        return 11;
    }
    lab.stop(leased.session_id); // already ended: idempotent
    const auto renewed = lab.start("starfield", "", nlohmann::json::object(), 20, RuntimeInputs::snapshot());
    fake_now += SceneLabRuntime::LeaseDuration - std::chrono::seconds(1);
    lab.heartbeat(renewed.session_id);
    fake_now += std::chrono::seconds(5);
    if (!lab.lease_active(renewed.generation)) {
        std::cerr << "heartbeat did not renew the Scene Lab lease\n";
        return 12;
    }
    lab.stop(renewed.session_id);
    lab.set_clock_for_testing({});

    plugins->delete_references();
    RuntimeInputs::clear_all();
    std::cout << "Scene Lab rejects stale generation writes\n";
    return 0;
}
