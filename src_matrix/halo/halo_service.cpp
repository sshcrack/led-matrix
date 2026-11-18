#include "halo_service.h"

#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>

#include "led-matrix.h"

namespace Halo {

namespace {
constexpr auto kTick = std::chrono::milliseconds(66);  // ~15 frames/s
constexpr auto kIdle = std::chrono::milliseconds(1000);
constexpr auto kAlignmentTime = std::chrono::seconds(20);
constexpr auto kReopenDelay = std::chrono::seconds(10);
// "off" frames after the halo is switched off, so the firmware fades out at
// once instead of waiting for its failsafe timeout.
constexpr int kOffFrames = 20;

// Opens a serial port write-only. Refuses names outside is_allowed_device() and
// anything that is not a tty: preopen() runs as root, so a bad setting must never
// make the daemon write frames into a regular file.
int open_uart(const std::string& device, int& error)
{
    if (!is_allowed_device(device)) {
        error = EPERM;
        return -1;
    }
    const int fd = ::open(device.c_str(), O_WRONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        error = errno;
        return -1;
    }
    termios tio{};
    if (!::isatty(fd) || tcgetattr(fd, &tio) != 0) {
        ::close(fd);
        error = ENOTTY;
        return -1;
    }
    cfmakeraw(&tio);
    cfsetospeed(&tio, B115200);
    cfsetispeed(&tio, B115200);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);
    tcsetattr(fd, TCSANOW, &tio);
    return fd;
}
}  // namespace

Service& Service::instance()
{
    static Service service;
    return service;
}

void Service::preopen(const std::string& config_file)
{
    std::string device = Settings{}.device;
    try {
        std::ifstream in(config_file);
        if (in) {
            const auto j = nlohmann::json::parse(in, nullptr, false);
            if (j.is_object() && j.contains("halo") && j["halo"].is_object() && j["halo"].contains("device") &&
                j["halo"]["device"].is_string())
                device = j["halo"]["device"].get<std::string>();
        }
    }
    catch (...) {
    }
    int error = 0;
    const int fd = open_uart(device, error);
    if (fd < 0) {
        spdlog::info("Halo: no UART at {} ({}), halo output stays off until it exists", device, std::strerror(error));
        return;
    }
    preopened_fd_ = fd;
    preopened_device_ = device;
    spdlog::info("Halo: opened {} before the privilege drop", device);
}

void Service::start(const Settings& settings)
{
    std::lock_guard lock(mutex_);
    if (thread_.joinable())
        return;
    settings_ = settings;
    test_until_ = {};
    stop_ = false;
    next_open_try_ = {};
    off_frames_left_ = 0;
    smoother_.reset();
    if (preopened_fd_ >= 0) {
        fd_ = preopened_fd_;
        fd_device_ = preopened_device_;
        preopened_fd_ = -1;
    }
    thread_ = std::thread([this] { run(); });
}

void Service::stop()
{
    {
        std::lock_guard lock(mutex_);
        if (!thread_.joinable())
            return;
        stop_ = true;
    }
    cv_.notify_all();
    thread_.join();
    sample_wanted_ = false;
    if (preopened_fd_ >= 0) {
        ::close(preopened_fd_);
        preopened_fd_ = -1;
    }
}

void Service::update_settings(const Settings& settings)
{
    {
        std::lock_guard lock(mutex_);
        settings_ = settings;
    }
    cv_.notify_all();
}

void Service::test_alignment(int led_count)
{
    {
        std::lock_guard lock(mutex_);
        test_led_count_ = led_count;
        test_until_ = led_count > 0 ? std::chrono::steady_clock::now() + kAlignmentTime : std::chrono::steady_clock::time_point{};
    }
    cv_.notify_all();
}

nlohmann::json Service::status()
{
    nlohmann::json j;
    {
        std::lock_guard lock(mutex_);
        j["settings"] = settings_;
        j["test_active"] = std::chrono::steady_clock::now() < test_until_;
    }
    {
        std::lock_guard lock(status_mutex_);
        j["error"] = last_error_.empty() ? nlohmann::json(nullptr) : nlohmann::json(last_error_);
    }
    j["frames_sent"] = frames_sent_.load();
    j["device_open"] = running_open_.load();
    return j;
}

void Service::submit_zones(const Zones& zones)
{
    std::lock_guard lock(zones_mutex_);
    latest_ = zones;
    sample_wanted_.store(false, std::memory_order_relaxed);
}

void Service::submit_black()
{
    submit_zones(Zones{});
}

void Service::sample(rgb_matrix::FrameCanvas* canvas, int width, int height)
{
    if (!canvas)
        return;
    int columns, depth;
    {
        std::lock_guard lock(mutex_);
        columns = settings_.sample_columns;
        depth = settings_.sample_depth;
    }
    const auto zones = sample_zones(
        width, height,
        [canvas](int x, int y) {
            Rgb c;
            canvas->GetPixel(x, y, &c.r, &c.g, &c.b);
            return c;
        },
        columns, depth);
    submit_zones(zones);
}

void Service::note_error(const std::string& message, bool warn)
{
    {
        std::lock_guard lock(status_mutex_);
        if (last_error_ == message)
            return;
        last_error_ = message;
    }
    if (warn)
        spdlog::warn("Halo: {}", message);
    else
        spdlog::info("Halo: {}", message);
}

void Service::close_fd()
{
    if (fd_ >= 0)
        ::close(fd_);
    fd_ = -1;
    running_open_ = false;
}

void Service::ensure_open(const std::string& device)
{
    if (fd_ >= 0 && fd_device_ != device) {
        close_fd();
        next_open_try_ = {};
    }
    if (fd_ >= 0) {
        running_open_ = true;
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now < next_open_try_)
        return;
    next_open_try_ = now + kReopenDelay;
    int error = 0;
    fd_ = open_uart(device, error);
    if (fd_ < 0) {
        // ENOENT is the normal state on the emulator and dev machines.
        note_error(std::string(error == ENOENT ? "no UART device " : "cannot open ") + device + ": " + std::strerror(error),
                   error != ENOENT);
        return;
    }
    fd_device_ = device;
    running_open_ = true;
    {
        std::lock_guard lock(status_mutex_);
        last_error_.clear();
    }
    spdlog::info("Halo: sending to {}", device);
}

void Service::send(std::uint8_t brightness, int led_count, std::span<const Rgb> zones)
{
    if (fd_ < 0)
        return;
    std::uint8_t rgb[3 * kMaxZones];
    for (size_t i = 0; i < zones.size(); ++i) {
        rgb[3 * i] = zones[i].r;
        rgb[3 * i + 1] = zones[i].g;
        rgb[3 * i + 2] = zones[i].b;
    }
    std::uint8_t buf[kMaxFrameBytes];
    const size_t n = encode(buf, brightness, static_cast<std::uint8_t>(led_count), static_cast<std::uint8_t>(zones.size()), rgb);
    if (n == 0)
        return;
    const ssize_t written = ::write(fd_, buf, n);
    if (written == static_cast<ssize_t>(n)) {
        ++frames_sent_;
        return;
    }
    if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return;  // the UART queue is full: drop this frame, the next one follows
    note_error(std::string("UART write failed: ") + (written < 0 ? std::strerror(errno) : "short write"), true);
    close_fd();
}

void Service::tick()
{
    Settings s;
    int test_led_count;
    {
        std::lock_guard lock(mutex_);
        s = settings_;
        test_led_count = std::chrono::steady_clock::now() < test_until_ ? test_led_count_ : 0;
    }
    if (test_led_count > 0) {
        sample_wanted_ = false;
        off_frames_left_ = kOffFrames;
        std::array<Rgb, kMaxZones> markers{};
        markers.front() = {255, 255, 255};
        if (test_led_count > 1) {
            // Put the green anchor near LED 1 even for short strips.
            const auto next = std::max(1, (kMaxZones + test_led_count / 2) / test_led_count);
            markers[next] = {0, 255, 0};
        }
        ensure_open(s.device);
        send(40, test_led_count, markers);
        return;
    }
    if (!s.enabled) {
        sample_wanted_ = false;
        smoother_.reset();
        if (off_frames_left_ > 0) {
            --off_frames_left_;
            ensure_open(s.device);
            send(0, s.led_count, Zones{});
        }
        return;
    }
    off_frames_left_ = kOffFrames;
    ensure_open(s.device);

    const auto brightness = static_cast<std::uint8_t>((s.brightness * 255 + 50) / 100);
    Zones zones{};
    if (s.mode == "fixed") {
        sample_wanted_ = false;
        smoother_.reset();
        Rgb c;
        parse_color(s.color, c.r, c.g, c.b);
        zones.fill(c);
    }
    else {
        Zones latest;
        {
            std::lock_guard lock(zones_mutex_);
            latest = latest_;
        }
        zones = smoother_.step(arrange_zones(latest, s.rotation, s.reverse));
        sample_wanted_.store(true, std::memory_order_relaxed);  // ask for a fresh sample before the next tick
    }
    if (s.mode == "match" && s.start_between_zones)
        send(brightness, s.led_count, start_between_zones(zones));
    else
        send(brightness, s.led_count, zones);
}

void Service::run()
{
    std::unique_lock lock(mutex_);
    while (!stop_) {
        const bool busy = settings_.enabled || off_frames_left_ > 0 || std::chrono::steady_clock::now() < test_until_;
        lock.unlock();
        tick();
        lock.lock();
        if (stop_)
            break;
        cv_.wait_for(lock, busy ? kTick : kIdle);
    }
    // Tell the firmware to fade out right away, then release the device.
    const Settings s = settings_;
    lock.unlock();
    send(0, s.led_count, Zones{});
    close_fd();
}

}  // namespace Halo
