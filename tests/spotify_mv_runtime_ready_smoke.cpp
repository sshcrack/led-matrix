#include <shared/matrix/input_ids.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/runtime_inputs.h>
#include <shared/matrix/server/common.h>

#include "matrix_control/AutomaticDirector.h"

#include <algorithm>
#include <iostream>


namespace {
class TestCoverScene final : public Scenes::Scene {
public:
    TestCoverScene()
    {
        update_default_properties();
        register_properties();
        load_properties(nlohmann::json::object());
    }
    bool render(rgb_matrix::FrameCanvas*) override { return true; }
    void register_properties() override {}
    std::string get_name() const override { return "spotify"; }
    Scenes::SceneDescriptor get_descriptor() const override
    {
        auto d = Scene::get_descriptor();
        d.automatic_eligible = true;
        d.family = "album-art";
        d.tags = {"music", "media", "album-art", "spotify"};
        d.intensity = 0.36f;
        d.motion = 0.28f;
        d.music_affinity = 1.0f;
        d.performance_cost = 0.68f;
        return d;
    }
    Scenes::SceneInputSpec get_runtime_input_spec() const override
    {
        Scenes::SceneInputSpec spec;
        spec.require(RuntimeInputIds::SpotifyPlayback);
        return spec;
    }
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }
};
}

int main()
{
    RuntimeInputs::clear_all();

    auto* manager = Plugins::PluginManager::instance();
    manager->initialize();

    auto plugins = manager->get_plugins();
    const auto it = std::find_if(plugins.begin(), plugins.end(), [](const auto* plugin) {
        return plugin != nullptr && plugin->get_plugin_name() == "SpotifyMV";
    });
    if (it == plugins.end()) {
        std::cerr << "SpotifyMV matrix plugin was not loaded\n";
        return 1;
    }

    if (!(*it)->requires_single_desktop_producer()) {
        std::cerr << "SpotifyMV did not opt into single desktop producer ownership\n";
        return 11;
    }
    Server::clear_desktop_producers();
    constexpr std::uint64_t old_desktop = 1001;
    constexpr std::uint64_t new_desktop = 1002;
    Server::register_desktop_producer(old_desktop);
    const auto ownership = Server::register_desktop_producer(new_desktop);
    if (!ownership.changed || ownership.previous_owner != old_desktop
        || ownership.owner != new_desktop
        || !Server::accepts_desktop_producer_message(new_desktop)
        || Server::accepts_desktop_producer_message(old_desktop)) {
        std::cerr << "newest desktop did not become the exclusive SpotifyMV producer\n";
        return 12;
    }

    (*it)->on_websocket_message("tools:ready");

    // Tool readiness itself is durable capability state. It must not disappear
    // while Automatic Mode waits for a suitable handoff point.
    auto readiness_snapshot = RuntimeInputs::snapshot();
    const auto* readiness = readiness_snapshot.find(RuntimeInputIds::SpotifyMVReady);
    if (readiness == nullptr || !readiness->available
        || !readiness_snapshot.boolean(RuntimeInputIds::SpotifyMVReady, "tools_ready").value_or(false)
        || readiness_snapshot.boolean(RuntimeInputIds::SpotifyMVReady, "first_frame_ready").value_or(true)) {
        std::cerr << "SpotifyMV tool readiness did not publish expected preparation state\n";
        return 2;
    }
    if (readiness->ttl_seconds.has_value()) {
        std::cerr << "SpotifyMV readiness unexpectedly expires after "
                  << *readiness->ttl_seconds << " seconds\n";
        return 3;
    }

    RuntimeInputs::set_available(RuntimeInputIds::Desktop, true, {{"connected", true}});
    RuntimeInputs::publish(
        RuntimeInputIds::SpotifyPlayback,
        {{"playing", true}, {"track_id", std::string("track-a")},
         {"track", std::string("Song")}, {"artist", std::string("Artist")},
         {"progress_ms", std::int64_t{90000}}, {"duration_ms", std::int64_t{200000}}},
        std::chrono::seconds(30));

    const auto wrappers = manager->get_scenes();
    const auto mv_wrapper = std::find_if(wrappers.begin(), wrappers.end(), [](const auto& candidate) {
        return candidate && candidate->get_name() == "spotifymv";
    });
    if (mv_wrapper == wrappers.end()) {
        std::cerr << "real SpotifyMV scene was not registered\n";
        return 4;
    }
    auto mv_unique = (*mv_wrapper)->create();
    mv_unique->update_default_properties();
    mv_unique->register_properties();
    mv_unique->load_properties(nlohmann::json::object());
    auto mv = std::shared_ptr<Scenes::Scene>(std::move(mv_unique));
    auto cover = std::make_shared<TestCoverScene>();

    // Automatic Mode prepares all candidate scenes while another scene is
    // visible. Prove the real SpotifyMV scene primes a track request without
    // ever entering render(). on_websocket_open() replays that pending request.
    mv->prepare_runtime(RuntimeInputs::snapshot());
    const auto replay = (*it)->on_websocket_open();
    const bool queued_track = replay.has_value() && std::any_of(replay->begin(), replay->end(), [](const std::string& message) {
        return message.starts_with("track:track-a:Song\nArtist\n");
    });
    if (!queued_track) {
        std::cerr << "inactive SpotifyMV scene did not queue background preparation for the current track\n";
        return 5;
    }

    (*it)->on_websocket_message("track:preparing:track-a");
    auto preparing = RuntimeInputs::snapshot();
    if (preparing.boolean(RuntimeInputIds::SpotifyMVReady, "first_frame_ready").value_or(true)) {
        std::cerr << "SpotifyMV preparation claimed a frame before desktop decode was ready\n";
        return 6;
    }

    (*it)->on_websocket_message("track:ready:track-a");
    const auto ready_snapshot = RuntimeInputs::snapshot();
    const auto* ready = ready_snapshot.find(RuntimeInputIds::SpotifyMVReady);
    if (ready == nullptr || !ready->available || ready->ttl_seconds.has_value()
        || !ready_snapshot.boolean(RuntimeInputIds::SpotifyMVReady, "first_frame_ready").value_or(false)
        || ready_snapshot.text(RuntimeInputIds::SpotifyMVReady, "track_id").value_or("") != "track-a") {
        std::cerr << "prepared SpotifyMV frame did not publish durable matching-track readiness\n";
        return 7;
    }

    // Model the server dispatcher: a stale/older desktop may still send a late
    // prepare message, but it is not the elected producer and therefore must
    // not be allowed to mutate the matrix-side SpotifyMV state.
    if (Server::accepts_desktop_producer_message(old_desktop))
        (*it)->on_websocket_message("track:preparing:track-a");
    const auto duplicate_preparing = RuntimeInputs::snapshot();
    if (!duplicate_preparing.boolean(RuntimeInputIds::SpotifyMVReady, "first_frame_ready").value_or(false)) {
        std::cerr << "standby desktop revoked an already-ready SpotifyMV track\n";
        return 10;
    }

    std::vector<std::shared_ptr<Scenes::Scene>> spotify_scenes{cover, mv};
    AutomaticDirector director(31);
    const auto live_inputs = RuntimeInputs::snapshot();
    const auto ranked = director.rank(spotify_scenes, live_inputs);
    if (ranked.size() != 2 || ranked.front().scene->get_name() != "spotifymv") {
        std::cerr << "real prepared SpotifyMV scene was not preferred mid-track\n";
        return 8;
    }
    const auto handoff = director.consider_switch(spotify_scenes, cover, live_inputs, 9000);
    if (!handoff.should_switch || !handoff.preferred_scene
        || handoff.preferred_scene->get_name() != "spotifymv") {
        std::cerr << "real prepared SpotifyMV scene did not trigger automatic mid-track handoff\n";
        return 9;
    }

    const auto failover = Server::unregister_desktop_producer(new_desktop);
    if (!failover.changed || failover.owner != old_desktop
        || !Server::accepts_desktop_producer_message(old_desktop)) {
        std::cerr << "SpotifyMV producer ownership did not fail over to the remaining desktop\n";
        return 13;
    }

    spotify_scenes.clear();
    mv.reset();
    cover.reset();
    manager->delete_references();
    Server::clear_desktop_producers();
    RuntimeInputs::clear_all();
    std::cout << "SpotifyMV readiness persists until an explicit state change\n";
    return 0;
}
