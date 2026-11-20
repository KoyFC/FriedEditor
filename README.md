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

## License

Fried Editor is released under the MIT License, see [`LICENSE`](LICENSE). Third party components are listed in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
