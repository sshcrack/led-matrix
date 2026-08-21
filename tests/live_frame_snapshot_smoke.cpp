#include "matrix_control/LiveFrameSnapshot.h"
#include "server/live_frame_protocol.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

int main()
{
    auto &store = LiveFrame::SnapshotStore::instance();
    std::vector<std::pair<LiveFrame::Snapshot, std::uint64_t>> published;
    store.set_publish_callback([&](const LiveFrame::Snapshot &snapshot,
                                   const std::uint64_t capture_generation) {
        published.emplace_back(snapshot, capture_generation);
    });

    // A connected transport alone must not cause any frame work.
    store.publish_solid_if_requested(2, 2, 1, 2, 3);
    if (!published.empty()) {
        std::cerr << "live frame published without viewer demand\n";
        return 1;
    }

    // Multiple viewers asking before one render coalesce into one capture, and
    // the capture reports the newest demand generation present at its start.
    const auto first_generation = store.request_capture();
    const auto second_generation = store.request_capture();
    store.publish_solid_if_requested(2, 2, 4, 5, 6);
    const std::vector<std::uint8_t> expected{
        4, 5, 6, 4, 5, 6,
        4, 5, 6, 4, 5, 6,
    };
    if (first_generation == 0 || second_generation <= first_generation ||
        published.size() != 1 || published.front().first.sequence != 1 ||
        published.front().first.width != 2 || published.front().first.height != 2 ||
        published.front().first.rgb != expected ||
        published.front().second != second_generation) {
        std::cerr << "coalesced live-frame publish was incorrect\n";
        return 2;
    }

    store.publish_solid_if_requested(2, 2, 7, 8, 9);
    if (published.size() != 1) {
        std::cerr << "live frame republished without new demand\n";
        return 3;
    }

    // Emulate demand arriving while a frame is being published. It must remain
    // outstanding after the older generation is marked served so the next
    // render performs another capture instead of losing or mis-tagging it.
    std::uint64_t raced_generation = 0;
    bool inject_racing_request = true;
    store.set_publish_callback([&](const LiveFrame::Snapshot &snapshot,
                                   const std::uint64_t capture_generation) {
        published.emplace_back(snapshot, capture_generation);
        if (inject_racing_request) {
            inject_racing_request = false;
            raced_generation = store.request_capture();
        }
    });

    const auto third_generation = store.request_capture();
    store.publish_solid_if_requested(1, 1, 10, 20, 30);
    if (published.size() != 2 || published.back().second != third_generation ||
        raced_generation <= third_generation) {
        std::cerr << "racing live-frame demand was not generation-isolated\n";
        return 4;
    }

    store.publish_solid_if_requested(1, 1, 40, 50, 60);
    if (published.size() != 3 || published.back().second != raced_generation ||
        published.back().first.sequence != 3) {
        std::cerr << "racing demand did not survive for the following frame\n";
        return 5;
    }

    const std::string encoded = Server::LiveFrameProtocol::encode(published.back().first);
    if (encoded.size() != 15 || encoded.substr(0, 4) != "LMF1" ||
        static_cast<unsigned char>(encoded[12]) != 40 ||
        static_cast<unsigned char>(encoded[13]) != 50 ||
        static_cast<unsigned char>(encoded[14]) != 60) {
        std::cerr << "LMF1 encoding changed unexpectedly\n";
        return 6;
    }

    std::cout << "live-frame capture is demand-driven, coalesced, and generation-safe\n";
    return 0;
}
