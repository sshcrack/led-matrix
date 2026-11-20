// RefreshMonitor's pure parts: /proc/interrupts parsing and window statistics.
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "matrix_control/refresh_monitor.h"

using namespace RefreshMonitor;

static int failures = 0;
#define CHECK(condition)                                                                \
    do {                                                                                \
        if (!(condition)) {                                                             \
            std::fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                                 \
        }                                                                               \
    } while (0)

// Shortened /proc/interrupts of a Pi 3B.
constexpr const char* kBefore = R"(           CPU0       CPU1       CPU2       CPU3
 51:    9561371    9564428    9539748    9483369 ARMCTRL-level  41 Edge      dwc_otg_sim-fiq
 74:     300514     309953     307369     301713 ARMCTRL-level  64 Edge      dwc_otg, dwc_otg_pcd, dwc_otg_hcd:usb1
104:    2503367    2388503    2434452    2469235 ARMCTRL-level  94 Edge      mmc1
180:    2402879    2546303    2627091   17226600 bcm2836-timer   1 Edge      arch_timer
IPI0:    594521     588930     401156        793       Rescheduling interrupts
Err:          0
)";
constexpr const char* kAfter = R"(           CPU0       CPU1       CPU2       CPU3
 51:    9561371    9564428    9539748    9483869 ARMCTRL-level  41 Edge      dwc_otg_sim-fiq
 74:     300514     309953     307369     301713 ARMCTRL-level  64 Edge      dwc_otg, dwc_otg_pcd, dwc_otg_hcd:usb1
104:    2503467    2388503    2434452    2470235 ARMCTRL-level  94 Edge      mmc1
180:    2402879    2546303    2627091   17227600 bcm2836-timer   1 Edge      arch_timer
IPI0:    594521     588930     401156        803       Rescheduling interrupts
Err:          0
)";

int main()
{
    const auto table = parse_interrupts(kBefore);
    CHECK(table.cpus == 4);
    CHECK(table.rows.size() == 5);  // Err has no per-CPU columns
    CHECK(table.rows[0].id == "51" && table.rows[0].name == "dwc_otg_sim-fiq" && table.rows[0].per_cpu[3] == 9483369);
    CHECK(table.rows[1].name == "dwc_otg, dwc_otg_pcd, dwc_otg_hcd:usb1");
    CHECK(table.rows[4].id == "IPI0" && table.rows[4].name == "Rescheduling interrupts");

    auto timing_before = std::make_shared<rgb_matrix::RefreshTiming>();
    auto timing_after = std::make_shared<rgb_matrix::RefreshTiming>();
    for (int i = 0; i < 900; ++i) timing_after->histogram[1100]++, timing_after->total_micros += 11000;
    for (int i = 0; i < 9; ++i) timing_after->histogram[2000]++, timing_after->total_micros += 20000;
    timing_after->refreshes = 909;
    timing_after->max_micros = 20000;

    const auto start = std::chrono::steady_clock::now();
    const Sample before{start, timing_before, parse_interrupts(kBefore)};
    const Sample after{start + std::chrono::seconds(10), timing_after, parse_interrupts(kAfter)};
    const auto w = window_json(before, after, 3);
    CHECK(w["seconds"] == 10.0);
    CHECK(w["refreshes"] == 909 && w["refresh_hz"] == 90.9);
    CHECK(w["median_us"] == 11005 && w["max_us"] == 20005 && w["late"] == 9);
    CHECK(w["late_per_1000"] == 9.9);
    const auto& irq = w["interrupts_per_second"];
    // CPU 3: 500 FIQ + 1000 mmc1 + 1000 timer in 10 s; IPIs separate.
    CHECK(irq["device_and_timer"][3] == 250.0 && irq["device_and_timer"][0] == 10.0);
    CHECK(irq["ipi"][3] == 1.0);
    CHECK(irq["top_on_refresh_cpu"][0]["per_second"] == 100.0);
    CHECK(irq["top_on_refresh_cpu"].size() == 4);
    CHECK(irq["top_on_refresh_cpu"][2]["name"] == "dwc_otg_sim-fiq");

    // No refresh thread (emulator): only interrupts.
    const Sample no_timing_before{start, nullptr, parse_interrupts(kBefore)};
    const Sample no_timing_after{start + std::chrono::seconds(10), nullptr, parse_interrupts(kAfter)};
    const auto e = window_json(no_timing_before, no_timing_after, 3);
    CHECK(!e.contains("refreshes") && e.contains("interrupts_per_second"));

    // Steady brightness: 1.2x the fastest refresh, never below 50 Hz.
    CHECK(steady_limit_hz(10405) == 80);
    CHECK(steady_limit_hz(16000) == 52);
    CHECK(steady_limit_hz(17000) == 0);
    CHECK(steady_limit_hz(0) == 0);

    if (failures)
        std::fprintf(stderr, "%d check(s) failed\n", failures);
    else
        std::puts("refresh_monitor_smoke: OK");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
