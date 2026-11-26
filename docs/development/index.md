# Development

## Tests

`ctest --test-dir build` runs the core's tests. `FRIED_ENGINE=<engine> build/tests/core_tests` also runs the tests that start the real player.

## Releasing

A release is a tag on GitHub with the notes of its version. The notes are kept in [`CHANGELOG.md`](https://github.com/KoyFC/FriedEditor/blob/main/CHANGELOG.md) while developing, and the Release workflow copies them into the release.

### While developing

Write each change worth telling under the topmost `## vX.Y.Z` heading of `CHANGELOG.md`, in `Added`, `Changed`, `Fixed` or `Removed`. That heading is the version in progress, so it has no number or date until it is published.

### Publishing a version

1. In `CHANGELOG.md`, rename `## vX.Y.Z` to the version and the date, for example `## v0.3.0 - 2026-12-01`, and read the notes once more.
2. To keep writing changes for the next version, add an empty `## vX.Y.Z` section above it.
3. Commit and push to `main`. The workflow builds what the branch has on GitHub.
4. On GitHub, open **Actions**, then **Release**, then **Run workflow**, and enter the version without the `v`, for example `0.3.0`. The editor reports that version.
5. The workflow builds and tests on Linux and Windows, then leaves a draft release `v<version>` with `FriedEditor-<version>-x86_64.AppImage` (SDL2 inside, OpenGL from the system), `FriedEditor-<version>-windows-x64.zip` (`fried_editor.exe` and `SDL2.dll`), `SHA256SUMS` (which the Project Manager pins to download it) and the notes of that section of `CHANGELOG.md`. It fails without creating anything if `CHANGELOG.md` has no section for that version, so the number in step 1 and the one in step 4 must be the same.
6. Open the draft under **Releases**, check the notes and publish it. The tag `v<version>` is created when the draft is published.

New interface follows the [interface conventions](ui-conventions.md).

The open work is in the [roadmap](roadmap.md).
