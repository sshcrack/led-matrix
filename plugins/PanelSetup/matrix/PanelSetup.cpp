#include "PanelSetup.h"

#include "scenes/PanelTestScene.h"
#include "shared/common/plugin_macros.h"

REGISTER_PLUGIN(PanelSetup, PanelSetup)

vector<std::unique_ptr<Plugins::SceneWrapper>> PanelSetup::create_scenes()
{
    vector<std::unique_ptr<Plugins::SceneWrapper>> created;
    created.push_back(std::make_unique<Scenes::PanelTestSceneWrapper>());
    return created;
}
