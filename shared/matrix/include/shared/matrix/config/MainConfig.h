#pragma once

#include <atomic>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include "data.h"
#include "nlohmann/json.hpp"
#include "shared/matrix/plugin/main.h"

using json = nlohmann::json;

namespace Config {
/// Outcome of a change to an entry looked up by id.
enum class Change { Saved, NotFound, SaveFailed };

/// The daemon's settings (config.json). Every setter saves its own change
/// (atomic and durable, see DurableFile::replace) before it returns, so a
/// power cut right after a reply cannot lose it. Setters return false
/// (or Change::SaveFailed) when the change was made in memory but could not
/// be written; callers report that instead of calling save themselves.
class MainConfig {
    mutable std::shared_mutex data_mutex;
    ConfigData::Root data;
    std::atomic<std::uint64_t> automatic_director_generation_{0};
    std::atomic<std::uint64_t> automatic_preferences_version_{0};

    std::mutex save_mutex;
    const std::string file_name;

    /// Writes the current data; serialized with save_mutex.
    bool persist();
    /// Runs fn(data) under the write lock; fn returns whether it changed
    /// anything. Saves when it did.
    template <class Fn>
    bool change(Fn&& fn);
    /// Same, for entries looked up by id: fn returns false when not found.
    template <class Fn>
    Change change_existing(Fn&& fn);

public:
    /// How the constructor treats a config file it cannot read.
    enum class LoadMode {
        /// Throw. For tools and tests that share someone else's file.
        Strict,
        /// The daemon's mode, so a file damaged by a power cut cannot keep the
        /// panel from starting: move the damaged file aside as
        /// `<file>.corrupt-<time>`, load `<file>.last-good` (refreshed after
        /// each good load) or else the defaults, and save the result.
        Recover,
    };

    explicit MainConfig(std::string filename, LoadMode mode = LoadMode::Strict);

    /// Set when `LoadMode::Recover` had to replace the config file: what was
    /// wrong and what was loaded instead. Empty after a normal load.
    [[nodiscard]] const std::string& load_recovery() const { return load_recovery_; }

    [[nodiscard]] static std::string last_good_path(const std::string& file_name);

private:
    std::string load_recovery_;

    void load_from_file(LoadMode mode);
    void migrate_presets();

public:
    std::string get_curr_id();
    std::string get_operation_mode();
    bool is_automatic_mode();
    bool set_operation_mode(const std::string& mode);
    std::uint64_t get_automatic_director_seed();
    std::uint64_t get_automatic_director_generation() const;
    bool set_automatic_director_seed(std::uint64_t seed);
    std::string get_automatic_director_mode();
    /// "mixed", "environment", "party" or "chill"; throws on anything else.
    bool set_automatic_director_mode(const std::string& mode);
    std::map<std::string, std::string> get_automatic_preferences();
    bool get_steady_brightness();
    bool set_steady_brightness(bool enabled);
    /// Increments whenever preferences change so consumers can cache them.
    std::uint64_t get_automatic_preferences_version() const;
    /// `preference` is "favorite", "on", "hidden" or "default" (removes it).
    bool set_automatic_preference(const std::string& look_key, const std::string& preference);
    ConfigData::SpotifyData get_spotify();

    std::shared_ptr<ConfigData::Preset> get_curr();

    std::map<std::string, std::shared_ptr<ConfigData::Preset>> get_presets();
    [[nodiscard]] std::map<std::string, std::string> get_plugin_configs() const;

    bool set_spotify(ConfigData::SpotifyData spotify);
    bool set_curr(std::string id);
    /// Selects a preset and switches to manual mode, in one save.
    bool play_preset(const std::string& id);

    Change delete_preset(const std::string& id);
    Change set_preset_display_name(const std::string& id, const std::string& display_name);

    bool set_presets(const std::string& id, std::shared_ptr<ConfigData::Preset> preset);

    bool set_plugin_config(const std::string& pluginId, const std::string& config);

    bool is_turned_off();
    bool set_turned_off(bool turned_off);

    // Schedule management methods
    std::map<std::string, ConfigData::Schedule> get_schedules();
    std::vector<ConfigData::CustomSceneVariant> get_custom_scene_variants(const std::string& scene_name);
    bool set_custom_scene_variant(const std::string& scene_name, const ConfigData::CustomSceneVariant& variant);
    Change delete_custom_scene_variant(const std::string& scene_name, const std::string& variant_id);
    bool set_schedule(const std::string& id, const ConfigData::Schedule& schedule);
    Change delete_schedule(const std::string& id);
    ConfigData::BrightnessSchedule get_brightness_schedule();
    bool set_brightness_schedule(ConfigData::BrightnessSchedule schedule);
    Environment::Settings get_environment();
    bool set_environment(Environment::Settings settings);
    Halo::Settings get_halo();
    /// Sanitizes and stores the settings without restarting the scene.
    bool set_halo(Halo::Settings settings);
    /// Caches an IP-detected location without touching the user's settings.
    bool set_environment_detected(double latitude, double longitude, std::string label);

    bool is_scheduling_enabled();
    bool set_scheduling_enabled(bool enabled);
    std::optional<std::string> get_active_scheduled_preset();

    // Update management methods
    ConfigData::UpdateSettings get_update_settings();
    bool set_update_settings(const ConfigData::UpdateSettings& settings);
    bool is_auto_update_enabled();
    bool set_auto_update_enabled(bool enabled);
    int get_update_check_interval_hours();
    bool set_update_check_interval_hours(int hours);
    tmillis_t get_last_check_time();
    bool set_last_check_time(tmillis_t time);

    /// Writes the current state again, for presets whose scenes were changed
    /// in place (shutdown). Setters save themselves.
    bool flush();
    /// Release plugin-owned scene instances after configuration has been
    /// persisted and before plugin DSOs are unloaded.
    void release_scene_references();
    std::string get_filename() const;
};
}