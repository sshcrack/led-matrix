# Automatic Director policy

Type: grilling
Status: complete
Blocked by: 02

## Resolved policy

Automatic Mode is the normal operation path. It builds a curated catalog from scenes that explicitly opt into `automatic_eligible`, expands their curated variants into independent looks, and ignores manual playlist weights. The Director ranks only Runtime-Input-eligible looks using normalized intensity/music/performance metadata, current audio/Spotify context, Pi render headroom, and scene/family recent-history penalties. Selection is seeded and stochastic only within the top ranked candidates so it stays varied without becoming chaotic.

Manual presets remain an advanced override. Activating a preset switches to manual mode. User-authored schedules temporarily take precedence over automatic operation without deleting the user's normal mode choice. Coordinator restarts clear transition carry-over so mode changes are immediate and safe.
