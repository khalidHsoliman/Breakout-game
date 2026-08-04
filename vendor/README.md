# Vendored dependencies

Committed to the repository so the project builds with no network access and no
manual dependency setup. Nothing here is modified, only trimmed.

| Library | Version | Source | Licence |
|---|---|---|---|
| GLFW | 3.4 | https://github.com/glfw/glfw/releases/tag/3.4 | Zlib/libpng, `glfw/LICENSE.md` |
| glad | 2.0.8 (generated) | https://gen.glad.sh | `(WTFPL OR CC0-1.0) AND Apache-2.0` |

There is no maths library. All maths in this project is hand-written.

GoogleTest is not vendored. It is fetched by CMake (pinned to v1.15.2 with a
SHA256 hash) and only when `BREAKOUT_BUILD_TESTS=ON`, so a failed download can
never block the game build.

## Regenerating glad

```
pip install glad2
glad --api gl:core=3.3 --extensions="" --out-path vendor/glad c
```

`--extensions=""` is deliberate. The renderer needs core OpenGL 3.3 only;
including all 623 extensions costs ~1.5 MB of generated code for nothing.

## Trimming

Removed from GLFW: `docs/`, `examples/`, `tests/`, `deps/`, `.github/`.
`deps/` only serves the examples and tests, which are forced off in
`vendor/CMakeLists.txt`. The licence and all build-relevant sources are
untouched. Total: ~1.9 MB.
