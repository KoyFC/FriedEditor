# Roadmap

FriedEditor is the scene editor for [Fried Engine](https://github.com/KoyFC/FriedEngine). Fried Project Manager creates and builds projects, the engine is the runtime, and this editor assembles scenes. The same roadmap is in [the engine's editor roadmap](https://koyfc.github.io/FriedEngine/development/editor-roadmap/). Items below say which repository owns them.

## Open work

### 1. Spikes (FriedEngine and here)

- [ ] A: loader round trip on PC and a console

## Known limits

Known limits: an object that other objects refer to cannot be deleted without fixing those references first, or the scene fails to load, and a game component is selected by its `width` and `height` or a fixed 16 pixel marker.

Known limit: a position that depends on the screen's size, such as the right edge, is set by a component, and the editor shows it unplaced because its player never updates.

## Later

A binary cooked scene format, prefabs, a `screen` field for dual-screen platforms, per-platform asset variants.
