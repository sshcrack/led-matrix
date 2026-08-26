#include <shared/desktop/DesktopStreamState.h>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void maybe_send(const DesktopStreamState& state, int& packet_count)
{
    if (state.can_send_udp())
        ++packet_count;
}
}  // namespace

int main()
{
    DesktopStreamState state;
    int packets = 0;

    require(!state.can_send_udp(), "UDP must be paused before the WebSocket is connected");

    state.on_websocket_open();
    require(!state.can_send_udp(), "UDP must wait for an explicit matrix-enabled status after connect");

    require(state.on_control_message("matrix_enabled:1"), "matrix enabled control message must be recognized");
    require(state.can_send_udp(), "UDP must resume when the connected matrix reports enabled");
    for (int i = 0; i < 5; ++i) maybe_send(state, packets);
    require(packets == 5, "enabled matrix must allow packet transmission");

    require(state.on_control_message("matrix_enabled:0"), "matrix disabled control message must be recognized");
    require(!state.can_send_udp(), "UDP must pause when the matrix reports disabled");
    for (int i = 0; i < 5; ++i) maybe_send(state, packets);
    require(packets == 5, "packet count must stop increasing while the matrix is off");

    require(!state.on_control_message("active:spotify"), "unrelated control messages must not change stream state");
    require(!state.can_send_udp(), "unrelated messages must not resume UDP");

    require(state.on_control_message("matrix_enabled:1"), "matrix re-enable status must be recognized");
    require(state.can_send_udp(), "matrix re-enable must resume UDP without restarting the desktop app");

    state.on_websocket_closed();
    require(!state.can_send_udp(), "UDP must pause immediately when the matrix connection is lost");

    require(state.on_control_message("matrix_enabled:1"), "stale matrix status must still parse deterministically");
    require(!state.can_send_udp(), "stale status while disconnected must never allow UDP");

    state.on_websocket_open();
    require(!state.can_send_udp(), "reconnect must require a fresh matrix-enabled handshake");

    std::cout << "desktop_stream_state_smoke: OK\n";
    return 0;
}
