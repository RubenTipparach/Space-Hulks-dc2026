---
name: Starmap cluster structure
description: The starmap.json represents a single cluster - game will have multiple clusters as progression
type: project
---

The starmap.json in levels/ represents a single cluster (cluster 1). The game progression involves multiple clusters, each with their own starmap. The current starmap has 10 nodes: 1 start (SECTOR ALPHA), 3 columns of normal sectors, and 1 boss node (HIVE VESSEL ALPHA).

**Why:** The game's full progression spans multiple clusters, each with a hand-designed or procedural starmap.
**How to apply:** When adding cluster progression, each cluster should have its own starmap JSON or generation parameters. Don't treat the current starmap as the only one.
