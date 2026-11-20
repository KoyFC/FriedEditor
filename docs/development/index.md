# Development

## Tests

`ctest --test-dir build` runs the core's tests. `FRIED_ENGINE=<engine> build/tests/core_tests` also runs the tests that start the real player.

## Releasing

The Release workflow is started by hand from the Actions tab. It asks for the version, which the editor then reports, and leaves a draft release `v<version>` with `FriedEditor-<version>-x86_64.AppImage` (SDL2 inside, OpenGL from the system), `FriedEditor-<version>-windows-x64.zip` (`fried_editor.exe` and `SDL2.dll`) and `SHA256SUMS`, which is what the Project Manager pins to download it.

The open work is in the [roadmap](roadmap.md).
