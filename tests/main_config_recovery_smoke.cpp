// Power-cut durability of the daemon's config (#217): saves go through
// DurableFile::replace (synced, permissions kept), and LoadMode::Recover turns a
// damaged config.json into the last good copy (or the defaults) instead of a
// daemon that cannot start, keeping the damaged file for inspection.
#include <shared/common/utils/durable_file.h>
#include <shared/matrix/config/MainConfig.h>
#include <spdlog/spdlog.h>
#include <sys/stat.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace {
int failures = 0;

void check(bool ok, const std::string& what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

std::string read(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::stringstream out;
    out << in.rdbuf();
    return out.str();
}

void write(const fs::path& path, const std::string& content)
{
    std::ofstream(path, std::ios::binary | std::ios::trunc) << content;
}

int count_with_prefix(const fs::path& dir, const std::string& prefix)
{
    int n = 0;
    for (const auto& entry : fs::directory_iterator(dir))
        if (entry.path().filename().string().starts_with(prefix))
            ++n;
    return n;
}

void remove_corrupt_copies(const fs::path& dir)
{
    for (const auto& entry : fs::directory_iterator(dir))
        if (entry.path().filename().string().starts_with("config.json.corrupt-"))
            fs::remove(entry.path());
}

/// Builds a good config with a recognisable preset name and returns its text.
std::string make_good_config(const std::string& path)
{
    Config::MainConfig cfg(path, Config::MainConfig::LoadMode::Recover);
    const auto presets = cfg.get_presets();
    check(cfg.set_preset_display_name(presets.begin()->first, "Living room") == Config::Change::Saved, "saving a good config");
    return read(path);
}

bool has_preset_named(Config::MainConfig& cfg, const std::string& name)
{
    for (const auto& [id, preset] : cfg.get_presets())
        if (preset && preset->display_name == name)
            return true;
    return false;
}
}  // namespace

int main()
{
    spdlog::set_level(spdlog::level::off);
    const auto dir = fs::temp_directory_path() / ("led-matrix-config-recovery-" + std::to_string(std::random_device{}()));
    fs::remove_all(dir);
    fs::create_directories(dir);
    const auto path = (dir / "config.json").string();
    const auto last_good = Config::MainConfig::last_good_path(path);

    // DurableFile::replace keeps an existing file's permissions and leaves no temp file.
    {
        const auto file = dir / "durable.txt";
        write(file, "old");
        fs::permissions(file, fs::perms::owner_read | fs::perms::owner_write);
        std::string error;
        check(DurableFile::replace(file, "new content", &error), "replace an existing file: " + error);
        check(read(file) == "new content", "replace writes the new content");
        struct stat st{};
        ::stat(file.c_str(), &st);
        check((st.st_mode & 07777) == 0600, "replace keeps the permissions of the replaced file");
        check(count_with_prefix(dir, "durable.txt.tmp-") == 0, "replace leaves no temp file");
        check(!DurableFile::replace(dir / "missing-dir" / "x", "x", &error) && !error.empty(),
              "replace into a missing directory fails with a reason");
    }

    // Stale temp files from an interrupted save are removed; a fresh one (a
    // concurrent writer's) is kept.
    {
        const auto stale = fs::path(DurableFile::temp_prefix(dir / "durable.txt") + "stale");
        const auto fresh = fs::path(DurableFile::temp_prefix(dir / "durable.txt") + "fresh");
        write(stale, "partial");
        write(fresh, "partial");
        fs::last_write_time(stale, fs::file_time_type::clock::now() - std::chrono::hours(1));
        check(DurableFile::remove_stale_temps(dir / "durable.txt") == 1, "one stale temp file removed");
        check(!fs::exists(stale) && fs::exists(fresh), "only the stale temp file is removed");
        fs::remove(fresh);
    }

    const auto good = make_good_config(path);

    // A good load refreshes the last good copy.
    {
        Config::MainConfig cfg(path, Config::MainConfig::LoadMode::Recover);
        check(cfg.load_recovery().empty(), "a good config needs no recovery");
        check(read(last_good) == good, "a good load stores the last good copy");
    }

    // Damage typical of a power cut: truncated, empty, zero-filled.
    const std::string damages[] = {good.substr(0, good.size() / 2), "", std::string(good.size(), '\0')};
    for (const auto& damaged : damages) {
        write(path, damaged);
        check(count_with_prefix(dir, "config.json.corrupt-") == 0, "no corrupt copy before the load");

        bool strict_threw = false;
        try {
            Config::MainConfig strict(path);
        }
        catch (const std::exception&) {
            strict_threw = true;
        }
        check(strict_threw, "Strict mode still throws on a damaged config");
        check(read(path) == damaged, "Strict mode leaves the damaged file alone");

        Config::MainConfig cfg(path, Config::MainConfig::LoadMode::Recover);
        check(!cfg.load_recovery().empty(), "recovery is reported");
        check(has_preset_named(cfg, "Living room"), "the last good copy is restored");
        check(count_with_prefix(dir, "config.json.corrupt-") == 1, "the damaged file is kept aside");
        Config::MainConfig reread(path);
        check(has_preset_named(reread, "Living room"), "the restored config is saved back to config.json");
        remove_corrupt_copies(dir);
    }

    // Both copies damaged: start from the defaults, keep the damaged file.
    {
        write(path, "{\"presets\":");
        write(last_good, "not json");
        Config::MainConfig cfg(path, Config::MainConfig::LoadMode::Recover);
        check(cfg.load_recovery().find("defaults") != std::string::npos, "falls back to the defaults");
        check(!cfg.get_presets().empty(), "the defaults include a preset");
        check(count_with_prefix(dir, "config.json.corrupt-") == 1, "the damaged file is kept aside");
        remove_corrupt_copies(dir);
    }

    // A deleted config.json is a deliberate reset, not damage: defaults, no restore.
    {
        make_good_config(path);
        {
            Config::MainConfig refresh(path, Config::MainConfig::LoadMode::Recover);
        }
        fs::remove(path);
        Config::MainConfig cfg(path, Config::MainConfig::LoadMode::Recover);
        check(cfg.load_recovery().empty(), "a missing config is not a recovery");
        check(!has_preset_named(cfg, "Living room"), "a missing config starts from the defaults");
    }

    fs::remove_all(dir);
    if (failures)
        return 1;
    std::cout << "ok\n";
    return 0;
}
