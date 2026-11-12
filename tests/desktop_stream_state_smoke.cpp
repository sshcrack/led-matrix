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

    // Backwards compatibility: old matrices only send matrix_enabled. A new
    // desktop must continue to work with them during a staggered upgrade.
    require(state.on_control_message("matrix_enabled:1"), "matrix enabled control message must be recognized");
    require(state.can_send_udp(), "legacy matrix-enabled handshake must still allow UDP");
    require(state.on_control_message("desktop_producer:1"), "producer ownership control message must be recognized");
    require(state.can_send_udp(), "active producer must keep UDP enabled");
    for (int i = 0; i < 5; ++i) maybe_send(state, packets);
    require(packets == 5, "enabled matrix must allow packet transmission");

    require(state.on_control_message("matrix_enabled:0"), "matrix disabled control message must be recognized");
    require(!state.can_send_udp(), "UDP must pause when the matrix reports disabled");
    for (int i = 0; i < 5; ++i) maybe_send(state, packets);
    require(packets == 5, "packet count must stop increasing while the matrix is off");

    require(!state.on_control_message("active:spotify"), "unrelated control messages must not change stream state");
    require(!state.can_send_udp(), "unrelated messages must not resume UDP");

    require(state.on_control_message("matrix_enabled:1"), "matrix re-enable status must be recognized");
    require(state.can_send_udp(), "matrix re-enable must resume UDP for the current producer");

    require(state.on_control_message("desktop_producer:0"), "producer standby control message must be recognized");
    require(!state.can_send_udp(), "standby desktop must never emit UDP");
    require(state.on_control_message("desktop_producer:1"), "producer re-election must be recognized");
    require(state.can_send_udp(), "promoted desktop must resume UDP without reconnecting");

    state.on_websocket_closed();
    require(!state.can_send_udp(), "UDP must pause immediately when the matrix connection is lost");

    require(state.on_control_message("matrix_enabled:1"), "stale matrix status must still parse deterministically");
    require(!state.can_send_udp(), "stale status while disconnected must never allow UDP");

    state.on_websocket_open();
    require(!state.can_send_udp(), "reconnect must require a fresh matrix-enabled handshake");
    require(state.on_control_message("desktop_producer:0"), "new matrix may put reconnecting desktop in standby before enabling it");
    require(state.on_control_message("matrix_enabled:1"), "fresh matrix enabled status must parse after reconnect");
    require(!state.can_send_udp(), "standby role received before matrix-enabled must keep UDP paused");

    // UI indication of the producer selection.
    using Role = DesktopStreamState::ProducerRole;
    DesktopStreamState roles;
    require(roles.producer_role() == Role::Disconnected, "fresh state must report disconnected");
    roles.on_websocket_open();
    require(roles.producer_role() == Role::Unreported, "connected without producer message must be unreported");
    require(roles.on_control_message("desktop_producer:1"), "producer message must parse");
    require(roles.producer_role() == Role::Selected, "desktop_producer:1 must show as selected");
    require(roles.on_control_message("desktop_producer:0"), "standby message must parse");
    require(roles.producer_role() == Role::Standby, "desktop_producer:0 must show as standby");
    require(!roles.matrix_enabled(), "matrix must not show enabled before matrix_enabled:1");
    require(roles.on_control_message("matrix_enabled:1"), "matrix enabled must parse");
    require(roles.matrix_enabled(), "matrix_enabled:1 must show enabled");
    roles.on_websocket_closed();
    require(roles.producer_role() == Role::Disconnected, "closed connection must report disconnected");
    require(!roles.matrix_enabled(), "closed connection must not show matrix enabled");
    roles.on_websocket_open();
    require(roles.producer_role() == Role::Unreported, "reconnect must forget the previous role");

    std::cout << "desktop_stream_state_smoke: OK\n";
    return 0;
}
