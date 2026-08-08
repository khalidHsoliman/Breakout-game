# Vendored dependencies

Committed to the repository so the project builds with no network access and no
manual dependency setup. Nothing here is modified, only trimmed.

| Library | Version | Source | Licence |
|---|---|---|---|
| GLFW | 3.4 | https://github.com/glfw/glfw/releases/tag/3.4 | Zlib/libpng, `glfw/LICENSE.md` |
| glad | 2.0.8 (generated) | https://gen.glad.sh | `(WTFPL OR CC0-1.0) AND Apache-2.0` |
| miniaudio | 0.11.25 | https://github.com/mackron/miniaudio | Public domain or MIT-0, stated in the header |

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

## miniaudio

A single header, so there is nothing to trim - but it is 4.1 MB of it, which is
most of what this directory now weighs. `miniaudio.c` is the one translation
unit that compiles the implementation, and it switches off what the game does
not use:

```c
#define MA_NO_ENCODING     /* playback only, nothing writes audio files */
#define MA_NO_GENERATION   /* no waveform or noise synthesis */
```

An audio library chosen for the same reason as everything else here: it is
source, it vendors, and it needs no DLL beside the executable. The alternative
considered was irrKlang, which the reference article for this renderer uses -
rejected because it ships as a closed-source binary under a non-commercial
licence, which would have cost the "clone it and build it, no binaries, no
network" property the rest of this directory exists to protect.

## Trimming

Removed from GLFW: `docs/`, `examples/`, `tests/`, `deps/`, `.github/`.
`deps/` only serves the examples and tests, which are forced off in
`vendor/CMakeLists.txt`. The licence and all build-relevant sources are
untouched. GLFW and glad together are ~1.9 MB; miniaudio is 4.1 MB on top.
