# FriedEditor

The scene editor for [Fried Engine](https://github.com/KoyFC/FriedEngine). See [`ROADMAP.md`](ROADMAP.md) for the plan and what is done.

## Layout

- `core/` is all of the editor's logic, with no UI toolkit in it: the standard library and the vendored `nlohmann/json` only. Nothing under it may include a UI header, and configuring fails if something does.
- `ui/qt/` is the Qt 6 backend, the only code that includes Qt. A backend draws, forwards input and holds no editor state beyond its widgets, so replacing the toolkit means writing another backend next to it.
- `app/` is the executable that starts a backend.
- `tests/` runs the core with doctest, and builds without any toolkit installed.
- `third_party/` holds `nlohmann/json` and `doctest`, unchanged.

## Building

```
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Without Qt 6 Widgets installed, only the core and its tests are built. `-DFRIED_EDITOR_QT=OFF` does the same on purpose.
