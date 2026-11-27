# FriedEditor

The scene editor for [Fried Engine](https://github.com/KoyFC/FriedEngine).

## Documentation

The documentation is at [koyfc.github.io/FriedEditor](https://koyfc.github.io/FriedEditor/).

- [Installing](https://koyfc.github.io/FriedEditor/installing/)
- [Using the editor](https://koyfc.github.io/FriedEditor/using/opening-a-project/)
- [Architecture](https://koyfc.github.io/FriedEditor/architecture/)
- [Development](https://koyfc.github.io/FriedEditor/development/)

## Building

Clone with `--recursive`, or run `git submodule update --init` once.

```
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Known issues

On Linux, in a Wayland session whose SDL2 is `sdl2-compat` (Arch and CachyOS install it as `sdl2`), the pointer is not locked while panning the view and does not wrap when dragging a value. Start the editor with `SDL_VIDEODRIVER=x11` to avoid it, see [The scene view](https://koyfc.github.io/FriedEditor/using/viewport/#panning-on-wayland).

## License

Fried Editor is released under the MIT License, see [`LICENSE`](LICENSE). Third party components are listed in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
