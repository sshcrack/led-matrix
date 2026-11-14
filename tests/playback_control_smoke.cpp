// Saving configuration must not restart the running scene (#197). Only the
// named intents in PlaybackControl do, and the last one is reported in
// /diagnostics (`playback.last_restart`).
#include <shared/matrix/config/MainConfig.h>
#include <shared/matrix/playback_control.h>
#include <shared/matrix/utils/shared.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <iostream>
#include <random>
#include <string>

namespace fs = std::filesystem;

namespace {
int failures = 0;

void check(bool ok, const std::string& what)
{
    if (ok)
        return;
    std::cerr << "FAIL: " << what << "\n";
    ++failures;
}

void expect_no_restart(const std::string& what)
{
    check(!PlaybackControl::restart_pending(), what + " requested a restart");
    check(PlaybackControl::diagnostics_json()["restarts"] == 0, what + " was counted as a restart");
}
}  // namespace

int main()
{
    spdlog::set_level(spdlog::level::off);
    const auto dir = fs::temp_directory_path() / ("led-matrix-playback-smoke-" + std::to_string(std::random_device{}()));
    fs::remove_all(dir);
    const auto path = (dir / "config.json").string();

    {
        Config::MainConfig cfg(path);
        PlaybackControl::reset_for_tests();
        check(PlaybackControl::diagnostics_json()["last_restart"].is_null(), "fresh state has no restart reason");

        // Non-playback saves: never touch the running scene.
        cfg.set_last_check_time(123456);
        expect_no_restart("set_last_check_time");

        cfg.set_automatic_director_seed(424242);
        expect_no_restart("set_automatic_director_seed");

        cfg.set_automatic_director_mode("chill");
        cfg.set_automatic_preference("boids/swarm", "favorite");
        expect_no_restart("automatic director mode and preferences");

        auto update = cfg.get_update_settings();
        update.auto_update_enabled = !update.auto_update_enabled;
        update.check_interval_hours = 6;
        cfg.set_update_settings(update);
        cfg.set_auto_update_enabled(true);
        cfg.set_update_check_interval_hours(12);
        expect_no_restart("update settings");

        auto environment = cfg.get_environment();
        environment.detected_label = "Somewhere";
        cfg.set_environment(environment);
        cfg.set_environment_detected(52.5, 13.4, "Berlin");
        expect_no_restart("environment");

        cfg.set_operation_mode("manual");
        cfg.set_turned_off(false);
        cfg.set_curr(cfg.get_curr_id());
        expect_no_restart("operation mode, power and preset selection saves");

        check(cfg.is_dirty(), "setters still mark the config dirty");
        check(cfg.save(), "config saves");
        expect_no_restart("save()");

        // Playback intents do restart, each with its reason.
        PlaybackControl::restart("power_toggle");
        check(PlaybackControl::restart_pending(), "restart() raises the flag the render loop polls");
        check(&PlaybackControl::restart_flag() == &exit_canvas_update, "render loop polls one flag");
        auto diagnostics = PlaybackControl::diagnostics_json();
        check(diagnostics["pending"] == true, "diagnostics report the pending restart");
        check(diagnostics["last_restart"]["reason"] == "power_toggle", "diagnostics name the restart reason");
        check(diagnostics["restarts"] == 1, "restart is counted");

        PlaybackControl::acknowledge();
        check(!PlaybackControl::restart_pending(), "acknowledge clears the request");
        check(PlaybackControl::diagnostics_json()["last_restart"]["reason"] == "power_toggle", "reason is kept after acknowledge");

        PlaybackControl::reload_preset("abc");
        check(PlaybackControl::restart_pending(), "reload_preset restarts");
        check(PlaybackControl::diagnostics_json()["last_restart"]["reason"] == "reload_preset:abc", "reload_preset names the preset");
        PlaybackControl::acknowledge();

        PlaybackControl::interrupt();
        check(PlaybackControl::restart_pending(), "interrupt wakes the render loop");
        PlaybackControl::acknowledge();
    }

    fs::remove_all(dir);
    if (failures) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "playback_control_smoke OK\n";
    return 0;
}
