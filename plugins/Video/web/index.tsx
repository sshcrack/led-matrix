import { definePluginUi } from '~/core/pluginUi'
import VideoPlaylistProperty from './VideoPlaylistProperty'

/**
 * Web UI for the Video plugin: the YouTube search + playlist editor for the
 * `video_playlist` control declared by VideoScene's `playlist` property
 * (`.control("video_playlist")` in plugins/Video/matrix/scenes/VideoScene.cpp).
 */
export default definePluginUi({
  plugin: 'Video',
  propertyControls: {
    video_playlist: ({ property, value, onChange }) => (
      <VideoPlaylistProperty property={property as never} value={(value ?? []) as string[]} onChange={onChange as (v: string[]) => void} />
    ),
  },
})
