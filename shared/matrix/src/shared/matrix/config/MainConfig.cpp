#include <shared/common/utils/durable_file.h>
#include <shared/matrix/config/MainConfig.h>
#include <shared/matrix/utils/shared.h>
#include <shared/matrix/utils/uuid.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <random>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "spdlog/spdlog.h"

using namespace spdlog;

namespace Config {
    namespace {
        bool is_uuid_like(const std::string &value) {
            if (value.size() != 36) {
                return false;
            }

            for (size_t i = 0; i < value.size(); ++i) {
                const bool is_hyphen = (i == 8 || i == 13 || i == 18 || i == 23);
                if (is_hyphen) {
                    if (value[i] != '-') {
                        return false;
                    }
                    continue;
                }

                const char c = value[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                    return false;
                }
            }

            return true;
        }

        ConfigData::Root parse_config(const std::string& text)
        {
            const json parsed = json::parse(text);
            if (!parsed.is_object())
                throw std::runtime_error("not a JSON object");
            return parsed.get<ConfigData::Root>();
        }

        std::optional<std::string> read_whole_file(const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return std::nullopt;
            return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }

        /// `<file>.corrupt-<UTC time>`, with a counter if that name is taken.
        std::filesystem::path corrupt_path(const std::string& file_name)
        {
            const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm utc{};
#ifdef _WIN32
            gmtime_s(&utc, &now);
#else
            gmtime_r(&now, &utc);
#endif
            char stamp[32];
            std::strftime(stamp, sizeof(stamp), "%Y%m%dT%H%M%SZ", &utc);
            std::filesystem::path path(file_name + ".corrupt-" + stamp);
            for (int i = 2; std::filesystem::exists(path); ++i) path = file_name + ".corrupt-" + stamp + "-" + std::to_string(i);
            return path;
        }
    }

    std::string MainConfig::last_good_path(const std::string& file_name)
    {
        return file_name + ".last-good";
    }

    // Saving never touches playback: call sites that need the running scene to
    // restart ask PlaybackControl (shared/matrix/playback_control.h).
    template <class Fn>
    bool MainConfig::change(Fn&& fn)
    {
        {
            std::unique_lock lock(this->data_mutex);
            if (!fn(this->data))
                return true;  // nothing changed, nothing to save
        }
        return this->persist();
    }

    template <class Fn>
    Change MainConfig::change_existing(Fn&& fn)
    {
        {
            std::unique_lock lock(this->data_mutex);
            if (!fn(this->data))
                return Change::NotFound;
        }
        return this->persist() ? Change::Saved : Change::SaveFailed;
    }

    string MainConfig::get_curr_id() {
        std::shared_lock lock(this->data_mutex);

        return this->data.curr;
    }

    string MainConfig::get_operation_mode() {
        std::shared_lock lock(this->data_mutex);
        return this->data.operation_mode;
    }

    bool MainConfig::is_automatic_mode() {
        std::shared_lock lock(this->data_mutex);
        // Schedules are explicit user-authored playback intent and therefore
        // temporarily take precedence over the normal automatic director.
        return this->data.operation_mode == "automatic" && !this->data.scheduling_enabled;
    }

    bool MainConfig::set_operation_mode(const string& mode)
    {
        if (mode != "automatic" && mode != "manual")
            throw std::invalid_argument("operation mode must be 'automatic' or 'manual'");
        return change([&](ConfigData::Root& d) { return std::exchange(d.operation_mode, mode) != mode; });
    }

    std::uint64_t MainConfig::get_automatic_director_seed() {
        std::shared_lock lock(this->data_mutex);
        return this->data.automatic_director_seed;
    }

    std::uint64_t MainConfig::get_automatic_director_generation() const {
        return automatic_director_generation_.load(std::memory_order_relaxed);
    }

    bool MainConfig::set_automatic_director_seed(std::uint64_t seed)
    {
        if (seed == 0) throw std::invalid_argument("automatic director seed must be non-zero");
        {
            std::unique_lock lock(this->data_mutex);
            this->data.automatic_director_seed = seed;
        }
        automatic_director_generation_.fetch_add(1, std::memory_order_relaxed);
        return this->persist();
    }

    string MainConfig::get_automatic_director_mode() {
        std::shared_lock lock(this->data_mutex);
        return this->data.automatic_director_mode;
    }

    bool MainConfig::set_automatic_director_mode(const string& mode)
    {
        if (mode != "mixed" && mode != "environment" && mode != "party" && mode != "chill")
            throw std::invalid_argument("mode must be 'mixed', 'environment', 'party' or 'chill'");
        return change([&](ConfigData::Root& d) { return std::exchange(d.automatic_director_mode, mode) != mode; });
    }

    bool MainConfig::get_steady_brightness()
    {
        std::shared_lock lock(this->data_mutex);
        return this->data.steady_brightness;
    }

    bool MainConfig::set_steady_brightness(bool enabled)
    {
        return change([&](ConfigData::Root& d) { return std::exchange(d.steady_brightness, enabled) != enabled; });
    }

    bool MainConfig::get_music_layer()
    {
        std::shared_lock lock(this->data_mutex);
        return this->data.music_layer;
    }

    bool MainConfig::set_music_layer(bool enabled)
    {
        return change([&](ConfigData::Root& d) { return std::exchange(d.music_layer, enabled) != enabled; });
    }

    std::map<string, string> MainConfig::get_automatic_preferences() {
        std::shared_lock lock(this->data_mutex);
        return this->data.automatic_preferences;
    }

    std::uint64_t MainConfig::get_automatic_preferences_version() const {
        return automatic_preferences_version_.load(std::memory_order_relaxed);
    }

    bool MainConfig::set_automatic_preference(const string& look_key, const string& preference)
    {
        if (look_key.empty())
            throw std::invalid_argument("look key must not be empty");
        if (preference != "favorite" && preference != "on" && preference != "hidden" && preference != "default")
            throw std::invalid_argument("preference must be 'favorite', 'on', 'hidden' or 'default'");
        {
            std::unique_lock lock(this->data_mutex);
            if (preference == "default")
                this->data.automatic_preferences.erase(look_key);
            else
                this->data.automatic_preferences[look_key] = preference;
        }
        automatic_preferences_version_.fetch_add(1, std::memory_order_relaxed);
        return this->persist();
    }

    std::shared_ptr<ConfigData::Preset> MainConfig::get_curr() {
        std::shared_lock lock(this->data_mutex);

        auto it = this->data.presets.find(data.curr);
        if (it == this->data.presets.end()) {
            return nullptr;
        }
        return it->second;
    }

    ConfigData::SpotifyData MainConfig::get_spotify() {
        std::shared_lock lock(this->data_mutex);

        return this->data.spotify;
    }

    bool MainConfig::set_spotify(ConfigData::SpotifyData spotify)
    {
        return change([&](ConfigData::Root& d) {
            d.spotify = std::move(spotify);
            return true;
        });
    }

    bool MainConfig::set_curr(string id)
    {
        return change([&](ConfigData::Root& d) { return std::exchange(d.curr, id) != id; });
    }

    bool MainConfig::play_preset(const string& id)
    {
        return change([&](ConfigData::Root& d) {
            const bool changed = d.curr != id || d.operation_mode != "manual";
            d.curr = id;
            d.operation_mode = "manual";
            return changed;
        });
    }

    Change MainConfig::delete_preset(const string& id)
    {
        return change_existing([&](ConfigData::Root& d) { return d.presets.erase(id) > 0; });
    }

    Change MainConfig::set_preset_display_name(const string& id, const string& display_name)
    {
        return change_existing([&](ConfigData::Root& d) {
            const auto it = d.presets.find(id);
            if (it == d.presets.end() || !it->second)
                return false;
            it->second->display_name = display_name;
            return true;
        });
    }

    bool MainConfig::set_presets(const string& id, std::shared_ptr<ConfigData::Preset> preset)
    {
        spdlog::info("Setting preset {}", id);
        return change([&](ConfigData::Root& d) {
            d.presets[id] = std::move(preset);
            return true;
        });
    }

    std::map<string, std::shared_ptr<ConfigData::Preset>> MainConfig::get_presets() {
        std::shared_lock lock(this->data_mutex);
        return this->data.presets;
    }

    bool MainConfig::flush()
    {
        return this->persist();
    }

    bool MainConfig::persist()
    {
        // One save at a time, and each one serializes the data after the
        // previous save finished: a slow earlier save can never overwrite the
        // file with older data than a later one wrote.
        std::lock_guard save_lock(this->save_mutex);
        try {
            string out;
            {
                std::shared_lock lock(this->data_mutex);
                out = json(this->data).dump(2);
            }
            debug("Saving config at '{}'", file_name);

            // Durable and atomic: readers (parallel scene workers, #181) and a
            // power cut see either the old or the new config, never a partial one.
            std::string write_error;
            if (!DurableFile::replace(file_name, out, &write_error)) {
                error("Could not save config: {}", write_error);
                return false;
            }
        }
        catch (std::exception& ex) {
            error("could not save config: {}", ex.what());
            return false;
        }

        return true;
    }

    void MainConfig::load_from_file(LoadMode mode)
    {
        const std::filesystem::path config_path(file_name);
        const auto parent = config_path.parent_path();
        if (!parent.empty()) {
            std::error_code directory_error;
            std::filesystem::create_directories(parent, directory_error);
            if (directory_error) {
                throw std::runtime_error(fmt::format(
                    "Could not create config directory '{}': {}",
                    parent.string(), directory_error.message()));
            }
        }

        if (mode == LoadMode::Recover) {
            if (const int removed = DurableFile::remove_stale_temps(config_path))
                warn("Removed {} unfinished config write(s) left by an interrupted save", removed);
        }

        if (!std::filesystem::exists(config_path)) {
            // Start from defaults; migrate_presets() then saves them (atomically).
            // A missing file is a deliberate reset (a rename never loses it), so
            // this does not restore the last good copy.
            debug("No config at '{}', starting from defaults", file_name);
            this->data = json::object().get<ConfigData::Root>();
            return;
        }

        const auto text = read_whole_file(config_path);
        if (!text) {
            // Unreadable (permissions), not damaged: never replace it.
            throw std::runtime_error(fmt::format(
                "Could not open config at '{}'", file_name));
        }

        std::string problem;
        try {
            this->data = parse_config(*text);
            if (mode == LoadMode::Recover) {
                const auto last_good = last_good_path(file_name);
                std::string write_error;
                if (read_whole_file(last_good) != text && !DurableFile::replace(last_good, *text, &write_error))
                    warn("Could not refresh the last good config copy: {}", write_error);
            }
            return;
        }
        catch (const std::exception& e) {
            if (mode == LoadMode::Strict)
                throw;
            problem = text->empty() ? std::string("the file was empty") : std::string(e.what());
        }

        // Keep the damaged file for inspection; it is never overwritten.
        const auto aside = corrupt_path(file_name);
        std::error_code ec;
        std::filesystem::rename(config_path, aside, ec);
        if (ec) {
            throw std::runtime_error(
                fmt::format("Config at '{}' is damaged ({}) and could not be moved aside: {}", file_name, problem, ec.message()));
        }
        problem = fmt::format("'{}' was damaged ({}); kept it as '{}'", file_name, problem, aside.string());

        const auto last_good = last_good_path(file_name);
        if (const auto backup = read_whole_file(last_good)) {
            try {
                this->data = parse_config(*backup);
                load_recovery_ = problem + fmt::format(" and restored '{}'", last_good);
                return;
            }
            catch (const std::exception& e) {
                problem += fmt::format("; '{}' was damaged too ({})", last_good, e.what());
            }
        }
        this->data = json::object().get<ConfigData::Root>();
        load_recovery_ = problem + "; started from the defaults";
    }

    void MainConfig::migrate_presets() {
        bool migrated = false;

        if (this->data.presets.empty()) {
            auto preset = ConfigData::Preset::create_default();
            const auto id = uuid::generate_uuid_v4();
            preset->display_name = "Default";
            this->data.presets[id] = std::move(preset);
            this->data.curr = id;
            migrated = true;
        }

        std::map<std::string, std::shared_ptr<ConfigData::Preset>> migrated_presets;
        std::unordered_map<std::string, std::string> id_map;

        for (const auto &[old_id, old_preset]: this->data.presets) {
            auto preset = old_preset;
            if (!preset) {
                preset = ConfigData::Preset::create_default();
                migrated = true;
            }

            std::string new_id = old_id;
            const bool keep_existing_id = is_uuid_like(old_id) && !migrated_presets.contains(old_id);

            if (!keep_existing_id) {
                do {
                    new_id = uuid::generate_uuid_v4();
                } while (migrated_presets.contains(new_id));

                id_map[old_id] = new_id;
                migrated = true;
            }

            if (preset->display_name.empty()) {
                preset->display_name = old_id;
                migrated = true;
            }

            migrated_presets[new_id] = std::move(preset);
        }

        if (migrated) {
            this->data.presets = std::move(migrated_presets);

            if (id_map.contains(this->data.curr)) {
                this->data.curr = id_map[this->data.curr];
            }

            if (!this->data.presets.contains(this->data.curr) && !this->data.presets.empty()) {
                this->data.curr = this->data.presets.begin()->first;
            }

            for (auto &[schedule_id, schedule]: this->data.schedules) {
                (void) schedule_id;
                if (id_map.contains(schedule.preset_id)) {
                    schedule.preset_id = id_map[schedule.preset_id];
                }
            }

            info("Migrated preset IDs to UUID keys and persisted updated config");
            this->persist();
        }
    }

    MainConfig::MainConfig(const string filename, LoadMode mode) : file_name(filename)
    {
        load_from_file(mode);
        if (!load_recovery_.empty()) {
            error("Recovered the config: {}", load_recovery_);
            this->persist();
        }
        migrate_presets();
        if (this->data.automatic_director_seed == 0) {
            std::random_device random;
            this->data.automatic_director_seed =
                (static_cast<std::uint64_t>(random()) << 32U) ^ static_cast<std::uint64_t>(random());
            if (this->data.automatic_director_seed == 0) this->data.automatic_director_seed = 1;
            info("Generated persistent Automatic Director seed {}", this->data.automatic_director_seed);
            this->persist();
        }
    }

    void MainConfig::release_scene_references() {
        std::unique_lock lock(data_mutex);
        for (auto &[id, preset] : data.presets) {
            if (preset) preset->scenes.clear();
        }
    }

    string MainConfig::get_filename() const {
        return this->file_name;
    }

    std::map<string, string> MainConfig::get_plugin_configs() const {
        std::shared_lock lock(this->data_mutex);
        return this->data.pluginConfigs;
    }

    bool MainConfig::set_plugin_config(const std::string& pluginId, const string& plugin_config)
    {
        return change([&](ConfigData::Root& d) { return std::exchange(d.pluginConfigs[pluginId], plugin_config) != plugin_config; });
    }

    bool MainConfig::is_turned_off() {
        return this->data.turned_off;
    }

    bool MainConfig::set_turned_off(bool turned_off)
    {
        if (this->data.turned_off.exchange(turned_off) == turned_off)
            return true;
        return this->persist();
    }

    std::map<string, ConfigData::Schedule> MainConfig::get_schedules() {
        std::shared_lock lock(this->data_mutex);
        return this->data.schedules;
    }

    vector<ConfigData::CustomSceneVariant> MainConfig::get_custom_scene_variants(const string &scene_name) {
        std::shared_lock lock(this->data_mutex);
        const auto it = this->data.custom_scene_variants.find(scene_name);
        return it == this->data.custom_scene_variants.end() ? vector<ConfigData::CustomSceneVariant>{} : it->second;
    }

    bool MainConfig::set_custom_scene_variant(const string& scene_name, const ConfigData::CustomSceneVariant& variant)
    {
        return change([&](ConfigData::Root& d) {
            auto& variants = d.custom_scene_variants[scene_name];
            const auto it = std::find_if(variants.begin(), variants.end(), [&](const auto& item) { return item.id == variant.id; });
            if (it == variants.end())
                variants.push_back(variant);
            else
                *it = variant;
            return true;
        });
    }

    Change MainConfig::delete_custom_scene_variant(const string& scene_name, const string& variant_id)
    {
        return change_existing([&](ConfigData::Root& d) {
            auto entry = d.custom_scene_variants.find(scene_name);
            if (entry == d.custom_scene_variants.end())
                return false;
            if (std::erase_if(entry->second, [&](const auto& item) { return item.id == variant_id; }) == 0)
                return false;
            if (entry->second.empty())
                d.custom_scene_variants.erase(entry);
            return true;
        });
    }

    bool MainConfig::set_schedule(const string& id, const ConfigData::Schedule& schedule)
    {
        return change([&](ConfigData::Root& d) {
            d.schedules[id] = schedule;
            return true;
        });
    }

    Change MainConfig::delete_schedule(const string& id)
    {
        return change_existing([&](ConfigData::Root& d) { return d.schedules.erase(id) > 0; });
    }

    ConfigData::BrightnessSchedule MainConfig::get_brightness_schedule() {
        std::shared_lock lock(this->data_mutex);
        return this->data.brightness_schedule;
    }

    bool MainConfig::set_brightness_schedule(ConfigData::BrightnessSchedule schedule)
    {
        schedule.sanitize();
        return change([&](ConfigData::Root& d) {
            d.brightness_schedule = std::move(schedule);
            return true;
        });
    }

    Environment::Settings MainConfig::get_environment() {
        std::shared_lock lock(this->data_mutex);
        return this->data.environment;
    }

    bool MainConfig::set_environment_detected(double latitude, double longitude, string label)
    {
        return change([&](ConfigData::Root& d) {
            auto& env = d.environment;
            const bool changed = env.detected_latitude != latitude || env.detected_longitude != longitude || env.detected_label != label;
            env.detected_latitude = latitude;
            env.detected_longitude = longitude;
            env.detected_label = std::move(label);
            return changed;
        });
    }

    bool MainConfig::set_environment(Environment::Settings settings)
    {
        settings.sanitize();
        // Scenes pick environment changes up live.
        return change([&](ConfigData::Root& d) {
            d.environment = std::move(settings);
            return true;
        });
    }

    Halo::Settings MainConfig::get_halo()
    {
        std::shared_lock lock(this->data_mutex);
        return this->data.halo;
    }

    bool MainConfig::set_halo(Halo::Settings settings)
    {
        settings.sanitize();
        return change([&](ConfigData::Root& d) {
            d.halo = std::move(settings);
            return true;
        });
    }

    bool MainConfig::is_scheduling_enabled() {
        std::shared_lock lock(this->data_mutex);
        return this->data.scheduling_enabled;
    }

    bool MainConfig::set_scheduling_enabled(bool enabled)
    {
        return change([&](ConfigData::Root& d) { return std::exchange(d.scheduling_enabled, enabled) != enabled; });
    }

    optional<string> MainConfig::get_active_scheduled_preset() {
        std::shared_lock lock(this->data_mutex);

        if (!this->data.scheduling_enabled) {
            return std::nullopt;
        }

        // Find all active schedules and sort by priority (shorter duration = higher priority)
        vector<std::pair<string, ConfigData::Schedule>> active_schedules;

        for (const auto& [id, schedule] : this->data.schedules) {
            if (schedule.is_active_now()) {
                active_schedules.emplace_back(id, schedule);
            }
        }

        if (active_schedules.empty()) {
            return std::nullopt;
        }

        // Sort by duration (ascending), so shorter schedules come first
        std::sort(active_schedules.begin(), active_schedules.end(),
             [](const auto& a, const auto& b) {
                 const auto& schedule_a = a.second;
                 const auto& schedule_b = b.second;

                 // Calculate duration in minutes for each schedule
                 int duration_a = (schedule_a.end_hour * 60 + schedule_a.end_minute) -
                                  (schedule_a.start_hour * 60 + schedule_a.start_minute);
                 int duration_b = (schedule_b.end_hour * 60 + schedule_b.end_minute) -
                                  (schedule_b.start_hour * 60 + schedule_b.start_minute);

                 // Handle schedules that cross midnight
                 if (duration_a < 0) duration_a += 24 * 60;  // Add 24 hours worth of minutes
                 if (duration_b < 0) duration_b += 24 * 60;

                 return duration_a < duration_b;  // Shorter duration = higher priority
             });

        // Return the preset_id of the highest priority (shortest duration) schedule
        return active_schedules[0].second.preset_id;
    }

    ConfigData::UpdateSettings MainConfig::get_update_settings() {
        std::shared_lock lock(this->data_mutex);
        return this->data.update_settings;
    }

    bool MainConfig::set_update_settings(const ConfigData::UpdateSettings& settings)
    {
        return change([&](ConfigData::Root& d) {
            d.update_settings = settings;
            return true;
        });
    }

    bool MainConfig::is_auto_update_enabled() {
        std::shared_lock lock(this->data_mutex);
        return this->data.update_settings.auto_update_enabled;
    }

    bool MainConfig::set_auto_update_enabled(bool enabled)
    {
        return change([&](ConfigData::Root& d) { return std::exchange(d.update_settings.auto_update_enabled, enabled) != enabled; });
    }

    int MainConfig::get_update_check_interval_hours() {
        std::shared_lock lock(this->data_mutex);
        return this->data.update_settings.check_interval_hours;
    }

    bool MainConfig::set_update_check_interval_hours(int hours)
    {
        return change([&](ConfigData::Root& d) { return std::exchange(d.update_settings.check_interval_hours, hours) != hours; });
    }

    tmillis_t MainConfig::get_last_check_time() {
        std::shared_lock lock(this->data_mutex);
        return this->data.update_settings.last_check_time;
    }

    bool MainConfig::set_last_check_time(tmillis_t time)
    {
        return change([&](ConfigData::Root& d) {
            d.update_settings.last_check_time = time;
            return true;
        });
    }
}
