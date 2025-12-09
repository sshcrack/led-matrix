#pragma once

#include "../VideoPlugin.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"

namespace Scenes {
class VideoScene : public Scene {
  static std::string lastUrlSent;
  VideoPlugin *plugin;

  bool showing_loading_animation = false;
  int loading_animation_frame = 0;
  bool streaming_enabled = false;

  size_t current_video_index = 0;
  tmillis_t last_switch_time = 0;

  void render_loading_animation();

public:
  VideoScene();
  ~VideoScene() override = default;

  bool render(rgb_matrix::RGBMatrixBase *matrix) override;

  string get_name() const override;

  void register_properties() override;

  tmillis_t get_default_duration() override { return 30000; }
  int get_default_weight() override { return 5; }

  void after_render_stop(RGBMatrixBase *matrix) override;

  // Properties
  PropertyPointer<std::vector<std::string>> video_urls =
      MAKE_STRING_LIST_PROPERTY(
          "video_urls",
          std::vector<std::string>({
              "https://www.youtube.com/watch?v=FtutLA63Cp8" // Bad Apple example
          }));

  PropertyPointer<bool> random_playback =
      MAKE_PROPERTY("random_playback", bool, false);

  [[nodiscard]] bool needs_desktop_app() const override { return true; }
};

class VideoSceneWrapper : public Plugins::SceneWrapper {
public:
  std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
};
} // namespace Scenes
