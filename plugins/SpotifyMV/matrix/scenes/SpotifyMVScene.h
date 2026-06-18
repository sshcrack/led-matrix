#pragma once

#include "../SpotifyMVPlugin.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/PropertyMacros.h"
#include "shared/matrix/wrappers.h"

namespace Scenes {

class SpotifyMVScene : public Scene {
public:
  SpotifyMVScene();
  ~SpotifyMVScene() override = default;

  bool render(rgb_matrix::FrameCanvas* canvas) override;
  std::string get_name() const override { return "spotifymv"; }
  void register_properties() override;
  void after_render_stop() override;
  bool needs_desktop_app() override { return true; }
  tmillis_t get_default_duration() override { return 210000; }
  int get_default_weight() override { return 5; }

  PropertyPointer<std::string> search_suffix =
      MAKE_PROPERTY("search_suffix", std::string, "official music video");
  PropertyPointer<bool> fallback_to_lyric_video =
      MAKE_PROPERTY("fallback_to_lyric_video", bool, true);

private:
  SpotifyMVPlugin* plugin_ = nullptr;
  std::string last_track_id_sent_;
  int loading_frame_ = 0;

  void render_loading(rgb_matrix::FrameCanvas* canvas, bool is_searching);
};

class SpotifyMVSceneWrapper : public Plugins::SceneWrapper {
public:
  std::unique_ptr<Scenes::Scene> create() override;
};

} // namespace Scenes
