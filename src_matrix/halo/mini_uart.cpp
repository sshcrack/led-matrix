#include "mini_uart.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Halo {

namespace {
constexpr std::uint32_t kReceiverEnable = 0x01;  // AUX_MU_CNTL_REG bit 0
constexpr std::uint64_t kControlRegister = 8;    // register index, scaled by iomem_reg_shift

std::string read_text(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    std::stringstream text;
    text << in.rdbuf();
    std::string s = text.str();
    // sysfs values end in '\n'; device tree strings in '\0' (and may hold several).
    while (!s.empty() && (s.back() == '\n' || s.back() == '\0')) s.pop_back();
    return s;
}

bool is_mini_uart(const std::filesystem::path& tty_dir)
{
    const std::string compatible = read_text(tty_dir / "device" / "of_node" / "compatible");
    std::stringstream entries(compatible);
    for (std::string entry; std::getline(entries, entry, '\0');)
        if (entry == "brcm,bcm2835-aux-uart")
            return true;
    return false;
}
}  // namespace

std::optional<std::uint64_t> mini_uart_control_register(const std::filesystem::path& tty_dir)
{
    if (!is_mini_uart(tty_dir))
        return std::nullopt;
    if (read_text(tty_dir / "console") == "Y")
        return std::nullopt;  // the kernel may read input from it
    const std::string base = read_text(tty_dir / "iomem_base");
    const std::string shift = read_text(tty_dir / "iomem_reg_shift");
    try {
        std::size_t used = 0;
        const std::uint64_t address = std::stoull(base, &used, 16);
        if (used != base.size() || address == 0)
            return std::nullopt;
        const unsigned long reg_shift = shift.empty() ? 0 : std::stoul(shift);
        if (reg_shift > 3)
            return std::nullopt;
        return address + (kControlRegister << reg_shift);
    }
    catch (...) {
        return std::nullopt;
    }
}

std::string disable_mini_uart_receiver(const std::string& device)
{
    std::error_code ec;
    const auto real = std::filesystem::canonical(device, ec);
    if (ec)
        return {};
    const std::string tty = real.filename().string();
    if (tty.rfind("ttyS", 0) != 0)
        return {};
    const auto address = mini_uart_control_register(std::filesystem::path("/sys/class/tty") / tty);
    if (!address)
        return {};

    const int fd = ::open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
    if (fd < 0)
        return "Halo: could not turn off the receiver of " + tty + " (/dev/mem: " + std::strerror(errno) + ")";
    const auto page_size = static_cast<std::uint64_t>(::sysconf(_SC_PAGESIZE));
    const std::uint64_t page = *address & ~(page_size - 1);
    void* map = ::mmap(nullptr, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, static_cast<off_t>(page));
    ::close(fd);
    if (map == MAP_FAILED)
        return "Halo: could not turn off the receiver of " + tty + " (mmap: " + std::strerror(errno) + ")";
    auto* control = reinterpret_cast<volatile std::uint32_t*>(static_cast<char*>(map) + (*address - page));
    const std::uint32_t before = *control;
    std::string line;
    if (before & kReceiverEnable) {
        *control = before & ~kReceiverEnable;
        line = "Halo: turned off the receiver of " + tty + " (TX only; its RX pin is the panel's E line)";
    }
    ::munmap(map, page_size);
    return line;
}

}  // namespace Halo
