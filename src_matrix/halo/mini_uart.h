#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace Halo {

/// Physical address of the BCM283x mini UART's AUX_MU_CNTL_REG for the tty whose
/// sysfs directory is `tty_dir` (e.g. /sys/class/tty/ttyS0), or nullopt when that
/// tty is not a mini UART (`brcm,bcm2835-aux-uart`) or is a kernel console.
std::optional<std::uint64_t> mini_uart_control_register(const std::filesystem::path& tty_dir);

/// The halo only transmits. On a 64-row panel the mini UART's RX pin (GPIO15) is
/// the panel's E address line, so an enabled receiver sees a break every
/// character time: ~12k interrupts/s that land on every core, the refresh core
/// included (measured on a Pi 3B: refresh rate 95 -> 67 Hz, visible dimming).
///
/// Clears the receiver enable bit of the mini UART behind `device` (symlinks such
/// as /dev/serial0 are followed). The 8250 driver never rewrites that register
/// outside RS485 mode, so the receiver stays off until reboot, also across closes
/// and reopens; the transmitter is unaffected. Needs root (/dev/mem). Does nothing
/// for other UARTs and for consoles. Returns a line for the log, empty when
/// there was nothing to do.
std::string disable_mini_uart_receiver(const std::string& device);

}  // namespace Halo
