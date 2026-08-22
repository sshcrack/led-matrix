# Music Director and transition intelligence

Type: prototype
Status: complete
Blocked by: 02, 04

## Resolved design

Music Director keeps its long-lived musical-state model (calm/groove/build/peak), section/drop reactions, dwell limits and beat/bar quantization, but now chooses the best curated Scene Variant for the target musical intensity rather than learning child-scene property knobs. Spotify artwork color support remains a shared visual-state input for compatible children.

Automatic scene-to-scene transitions are planned centrally from the outgoing/incoming scene profiles plus the freshest rich AudioState. The planner chooses a visual transition family, beat-quantized duration and a short live-rendered wait to the next reliable beat. Drops into higher energy use an immediate glitch cut. If tempo confidence/stability is weak or audio is absent, configured fallback timing remains intact. Automatic next-scene selection is deferred until the current scene actually ends so a 30-second-old context cannot preselect a stale music target.
