#include "refresh_monitor.h"

#include <led-matrix.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include "shared/matrix/config/MainConfig.h"
#include "shared/matrix/diagnostics.h"

namespace RefreshMonitor {

namespace {
constexpr auto kInterval = std::chrono::seconds(10);
constexpr std::size_t kSamples = 7;  // 60 s between the oldest and the newest
constexpr std::size_t kTopInterrupts = 6;
constexpr auto kTick = std::chrono::seconds(1);  // how fast a settings change takes effect
// Long enough for a few hundred refreshes; the fastest of them is the
// uncontended refresh time even when something else is busy meanwhile.
constexpr auto kCalibration = std::chrono::seconds(3);
constexpr double kSteadyHeadroom = 1.2;
constexpr int kMinimumSteadyHz = 50;

const char* pacing_name(int state)
{
    static constexpr const char* names[] = {"off", "calibrating", "on", "too_slow", "unavailable"};
    return names[state];
}

std::string trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return std::string(s);
}

double rounded(double value, double step = 0.1)
{
    return static_cast<long long>(value / step + 0.5) * step;
}
}  // namespace

InterruptTable parse_interrupts(std::string_view text)
{
    InterruptTable table;
    std::istringstream in{std::string(text)};
    std::string line;
    bool header = true;
    while (std::getline(in, line)) {
        if (header) {
            // "           CPU0       CPU1 ..."
            std::istringstream words(line);
            std::string word;
            while (words >> word)
                if (word.rfind("CPU", 0) == 0)
                    ++table.cpus;
            header = false;
            continue;
        }
        const auto colon = line.find(':');
        if (colon == std::string::npos || table.cpus == 0)
            continue;
        InterruptRow row;
        row.id = trim(std::string_view(line).substr(0, colon));
        std::istringstream rest(line.substr(colon + 1));
        for (int cpu = 0; cpu < table.cpus; ++cpu) {
            std::uint64_t count = 0;
            if (!(rest >> count))
                break;
            row.per_cpu.push_back(count);
        }
        if (row.per_cpu.size() != static_cast<std::size_t>(table.cpus))
            continue;  // "Err: 0" style lines without per-CPU columns
        std::string remainder;
        std::getline(rest, remainder);
        // Device lines: "<chip> <hwirq> <trigger> <name>"; IPI lines: "<description>".
        std::istringstream fields(remainder);
        std::vector<std::string> words;
        for (std::string w; fields >> w;) words.push_back(w);
        const bool numeric = !row.id.empty() && std::all_of(row.id.begin(), row.id.end(), ::isdigit);
        const std::size_t skip = numeric && words.size() > 3 ? 3 : 0;
        std::string name;
        for (std::size_t i = skip; i < words.size(); ++i) name += (name.empty() ? "" : " ") + words[i];
        row.name = name;
        table.rows.push_back(std::move(row));
    }
    return table;
}

nlohmann::json window_json(const Sample& before, const Sample& after, int refresh_cpu)
{
    const double seconds = std::chrono::duration<double>(after.at - before.at).count();
    nlohmann::json w = {{"seconds", rounded(seconds)}};
    if (seconds <= 0)
        return w;

    if (before.timing && after.timing) {
        const auto t = rgb_matrix::RefreshTimingWindow::Between(*before.timing, *after.timing);
        w["refreshes"] = t.refreshes;
        w["refresh_hz"] = rounded(static_cast<double>(t.refreshes) / seconds);
        w["average_us"] = rounded(t.average_micros);
        w["min_us"] = t.min_micros;
        w["median_us"] = t.median_micros;
        w["p99_us"] = t.p99_micros;
        w["max_us"] = t.max_micros;
        w["late"] = t.late;  // longer than 1.5 x median
        w["late_per_1000"] = t.refreshes ? rounded(1000.0 * static_cast<double>(t.late) / static_cast<double>(t.refreshes), 0.01) : 0.0;
    }

    // Interrupt deltas per CPU; IPIs (scheduler/function calls) are kept apart
    // from device and timer interrupts.
    const auto& a = before.interrupts;
    const auto& b = after.interrupts;
    if (b.cpus > 0 && a.cpus == b.cpus) {
        std::vector<double> device(static_cast<std::size_t>(b.cpus), 0.0);
        std::vector<double> ipi(static_cast<std::size_t>(b.cpus), 0.0);
        struct Hit {
            std::string id, name;
            double per_second;
        };
        std::vector<Hit> on_refresh_cpu;
        for (const auto& row : b.rows) {
            const auto previous = std::find_if(a.rows.begin(), a.rows.end(), [&](const InterruptRow& r) { return r.id == row.id; });
            const bool is_ipi = row.id.rfind("IPI", 0) == 0;
            for (int cpu = 0; cpu < b.cpus; ++cpu) {
                const auto now = row.per_cpu[static_cast<std::size_t>(cpu)];
                const auto then = previous != a.rows.end() ? previous->per_cpu[static_cast<std::size_t>(cpu)] : 0;
                const double rate = static_cast<double>(now >= then ? now - then : 0) / seconds;
                (is_ipi ? ipi : device)[static_cast<std::size_t>(cpu)] += rate;
                if (cpu == refresh_cpu && rate > 0)
                    on_refresh_cpu.push_back({row.id, row.name, rate});
            }
        }
        std::sort(on_refresh_cpu.begin(), on_refresh_cpu.end(), [](const Hit& x, const Hit& y) { return x.per_second > y.per_second; });
        nlohmann::json top = nlohmann::json::array();
        for (std::size_t i = 0; i < on_refresh_cpu.size() && i < kTopInterrupts; ++i)
            top.push_back(
                {{"irq", on_refresh_cpu[i].id}, {"name", on_refresh_cpu[i].name}, {"per_second", rounded(on_refresh_cpu[i].per_second)}});
        nlohmann::json device_json = nlohmann::json::array(), ipi_json = nlohmann::json::array();
        for (int cpu = 0; cpu < b.cpus; ++cpu) {
            device_json.push_back(rounded(device[static_cast<std::size_t>(cpu)]));
            ipi_json.push_back(rounded(ipi[static_cast<std::size_t>(cpu)]));
        }
        w["interrupts_per_second"] = {{"device_and_timer", device_json}, {"ipi", ipi_json}, {"top_on_refresh_cpu", top}};
    }
    return w;
}

int steady_limit_hz(std::uint32_t fastest_us)
{
    if (fastest_us == 0)
        return 0;
    const int hz = static_cast<int>(1e6 / (static_cast<double>(fastest_us) * kSteadyHeadroom));
    return hz >= kMinimumSteadyHz ? hz : 0;
}

Monitor::Monitor(rgb_matrix::RGBMatrixBase* matrix, int refresh_cpu, int configured_limit_hz)
    : matrix_(matrix),
      hardware_(dynamic_cast<rgb_matrix::RGBMatrix*>(matrix)),
      refresh_cpu_(refresh_cpu),
      configured_limit_hz_(configured_limit_hz)
{
    samples_.push_back(take_sample());
    thread_ = std::thread([this] { run(); });
}

Monitor::~Monitor()
{
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
}

void Monitor::attach_config(Config::MainConfig* config)
{
    config_.store(config);
    cv_.notify_all();
}

Sample Monitor::take_sample() const
{
    Sample sample;
    sample.at = std::chrono::steady_clock::now();
    if (hardware_) {
        auto timing = std::make_shared<rgb_matrix::RefreshTiming>();
        if (hardware_->GetRefreshTiming(timing.get()))
            sample.timing = std::move(timing);
    }
    if (std::ifstream file("/proc/interrupts"); file) {
        std::stringstream text;
        text << file.rdbuf();
        sample.interrupts = parse_interrupts(text.str());
    }
    return sample;
}

bool Monitor::update_pacing(std::chrono::steady_clock::time_point now)
{
    auto* config = config_.load();
    if (!config)
        return false;
    const bool wanted = config->get_steady_brightness();
    const Pacing before = pacing_;
    if (!wanted) {
        if (pacing_ != Pacing::Off) {
            if (hardware_ && pacing_ != Pacing::Unavailable)
                hardware_->SetRefreshLimit(configured_limit_hz_);
            pacing_ = Pacing::Off;
            pacing_limit_hz_ = 0;
            spdlog::info("Steady brightness off");
        }
        return pacing_ != before;
    }
    auto snapshot = [this] {
        auto timing = std::make_shared<rgb_matrix::RefreshTiming>();
        return hardware_ && hardware_->GetRefreshTiming(timing.get()) ? timing : nullptr;
    };
    if (pacing_ == Pacing::Off) {
        // Measure without any limit: a padded refresh says nothing about the panel.
        if (!hardware_) {
            pacing_ = Pacing::Unavailable;
            return true;
        }
        hardware_->SetRefreshLimit(0);
        calibration_before_ = snapshot();
        calibration_started_ = now;
        pacing_ = Pacing::Calibrating;
        return true;
    }
    if (pacing_ == Pacing::Calibrating && now - calibration_started_ >= kCalibration) {
        const auto after = snapshot();
        if (!after || !calibration_before_) {
            pacing_ = Pacing::Unavailable;
            return true;
        }
        const auto window = rgb_matrix::RefreshTimingWindow::Between(*calibration_before_, *after);
        pacing_fastest_us_ = window.min_micros;
        pacing_limit_hz_ = steady_limit_hz(window.min_micros);
        if (pacing_limit_hz_ > 0) {
            hardware_->SetRefreshLimit(pacing_limit_hz_);
            pacing_ = Pacing::On;
            spdlog::info("Steady brightness: refresh paced to {} Hz (fastest refresh {} us)", pacing_limit_hz_, pacing_fastest_us_);
        }
        else {
            hardware_->SetRefreshLimit(configured_limit_hz_);
            pacing_ = Pacing::TooSlow;
            spdlog::warn("Steady brightness: the panel refreshes too slowly ({} us) to pace it above {} Hz; left unpaced",
                         pacing_fastest_us_, kMinimumSteadyHz);
        }
    }
    return pacing_ != before;
}

void Monitor::run()
{
    auto next_sample = std::chrono::steady_clock::now() + kInterval;
    std::unique_lock lock(mutex_);
    while (!cv_.wait_for(lock, kTick, [this] { return stop_; })) {
        lock.unlock();
        const auto now = std::chrono::steady_clock::now();
        bool changed = update_pacing(now);
        if (now >= next_sample) {
            next_sample += kInterval;
            samples_.push_back(take_sample());
            while (samples_.size() > kSamples) samples_.pop_front();
            changed = true;
        }
        if (changed)
            publish();
        lock.lock();
    }
}

void Monitor::publish()
{
    const auto& newest = samples_.back();
    nlohmann::json state = {{"refresh_cpu", refresh_cpu_}, {"timing_available", static_cast<bool>(newest.timing)}};
    if (newest.timing)
        state["max_us_since_start"] = newest.timing->max_micros;
    nlohmann::json windows = nlohmann::json::object();
    if (samples_.size() >= 2)
        windows["10s"] = window_json(samples_[samples_.size() - 2], newest, refresh_cpu_);
    if (samples_.size() == kSamples)
        windows["60s"] = window_json(samples_.front(), newest, refresh_cpu_);
    state["windows"] = std::move(windows);
    nlohmann::json steady = {{"enabled", pacing_ != Pacing::Off}, {"state", pacing_name(static_cast<int>(pacing_))}};
    if (pacing_ == Pacing::On)
        steady["limit_hz"] = pacing_limit_hz_;
    if (pacing_fastest_us_)
        steady["fastest_refresh_us"] = pacing_fastest_us_;
    state["steady_brightness"] = std::move(steady);
    Diagnostics::RuntimeDiagnostics::instance().set_refresh_state(std::move(state));
}

}  // namespace RefreshMonitor
