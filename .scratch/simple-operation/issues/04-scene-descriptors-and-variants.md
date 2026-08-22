# Scene descriptors and curated variants

Type: prototype
Status: complete
Blocked by: 02

## Question

What is the smallest scene/plugin descriptor and curated-variant model that lets the UI and directors understand mood, intensity, runtime requirements, performance cost, and good-looking property presets without exposing low-level knobs?

## Direction already decided

This is the next implementation phase. The goal is not to add configuration. Add declarative metadata and curated Scene Variants so Automatic Mode can make good choices without user-authored weights. The manifest should become the source the web UI and future directors can query for scene mood/intensity/capabilities/performance hints/tags/variants. Keep manual low-level properties as an advanced escape hatch.
