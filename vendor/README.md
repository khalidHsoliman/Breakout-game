# Vendored dependencies

Committed to the repository so the project builds with no network access and no
manual dependency setup. Nothing here is modified, only trimmed.

| Library | Version | Source | Licence |
|---|---|---|---|
| GLFW | 3.4 | https://github.com/glfw/glfw/releases/tag/3.4 | Zlib/libpng, `glfw/LICENSE.md` |
| glad | 2.0.8 (generated) | https://gen.glad.sh | `(WTFPL OR CC0-1.0) AND Apache-2.0` |
| miniaudio | 0.11.25 | https://github.com/mackron/miniaudio | Public domain or MIT-0, stated in the header |
| GoogleTest | 1.15.2 | https://github.com/google/googletest | BSD-3-Clause, `googletest/LICENSE` |

There is no maths library. All maths in this project is hand-written.

GoogleTest was fetched by CMake until late on. That was a mistake: the fetch
runs at *configure* time for the whole project, so a machine with no network
failed before anything was generated and took the game down with it, not just
the tests. It is vendored now, and configuring needs no network at all.

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

## GoogleTest

`googlemock/` is not here - only `gtest` is used - and neither are the docs, CI
scripts, Bazel files or GoogleTest's own test suite. `BUILD_GMOCK` is forced off
in `tests/CMakeLists.txt`, so nothing looks for the missing directory. What is
left is the library, its headers and the licence: 2.1 MB of the original 4.4.

## Trimming

Removed from GLFW: `docs/`, `examples/`, `tests/`, `deps/`, `.github/`.
`deps/` only serves the examples and tests, which are forced off in
`vendor/CMakeLists.txt`. The licence and all build-relevant sources are
untouched. GLFW and glad together are ~1.9 MB; miniaudio is 4.1 MB on top.
