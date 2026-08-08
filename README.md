# Breakout

![build](https://github.com/khalidHsoliman/Breakout-game/actions/workflows/ci.yml/badge.svg)

A Breakout clone in C++, written from scratch over seven days. No engine, no
gameplay framework, and no maths library: the only dependencies are GLFW for the
window, glad for the OpenGL loader and miniaudio for sound, all three vendored
as source, plus GoogleTest fetched at configure time.

It ships with an in-game level editor. Levels are plain text, so one built with
the mouse, one typed in Notepad and the one built into the game are the same
kind of thing.

## Building

Requires Visual Studio 2022 and CMake 3.21 or newer. The first configure
downloads GoogleTest, so it needs a network connection; everything else is in
the repository.

```
git clone https://github.com/khalidHsoliman/Breakout-game.git
cd Breakout-game
cmake --preset vs2022-x64
cmake --build build --config Release
```

The executable lands in `build/bin/Release/breakout.exe`.

To build without the test target - which also skips the GoogleTest download, so
it works offline - use the `vs2022-x64-notests` preset instead.

## Running the tests

```
ctest --test-dir build -C Debug
```

154 tests, green in both Debug and Release. CI runs both configurations on every
push.

## Controls

| Key | In the game |
|---|---|
| Left / Right | Move the paddle |
| Space | Launch the ball |
| P | Pause |
| R | Restart, after a win or a loss |
| E | Open the level editor |
| F11 | Toggle fullscreen |
| Escape | Quit |

| Input | In the editor |
|---|---|
| Left click | Cycle a cell: empty, 1, 2, 3, 4, empty |
| Right click | Clear a cell |
| S | Save to `custom-level.txt`, beside the executable |
| L | Load it back |
| Enter | Play the level on screen |
| E | Back to the game, discarding unsaved edits |

A brick's colour is its remaining toughness, so a level can be read at a glance
and a damaged brick recolours as it is hit.

## How it is put together

Four layers, with the dependency direction enforced by CMake target linkage
rather than by convention:

```
                 breakout (main.cpp)
                 /                 \
        platform                    game
        Window, Renderer,           World, Systems, Level,
        Viewport, Font,             Spawn, Editor, LevelLayout
        FileSystem
                 \                 /
                       core
                       Entity, ComponentStore, Color
                        |
                       math
                       Vec2, AABB, Circle, Collision
```

`platform` and `game` are **siblings**. Neither links the other, so the game
cannot draw and the renderer cannot know what a brick is; `main.cpp` is the only
place they meet. The include directory for glad is `PRIVATE` to the platform
target, so a stray `#include <glad/gl.h>` in the game layer does not compile.

Entities are a strongly-typed id and components live in sparse sets - a dense
array for iteration, a sparse index for lookup. Systems are free functions over
the `World`. The game advances on a fixed 1/60 timestep with an accumulator, so
behaviour does not change with frame rate; a test asserts that 600 steps produce
identical results twice over.

Rendering is one dynamic vertex buffer and a single draw call per frame. Text is
a hand-encoded 5x7 bitmap font drawn as one quad per lit pixel through that same
batch, so there is no font library and no texture path. The play area keeps its
aspect ratio on any display, with black bars where the shapes disagree.

Sound works the same way the input does, in the opposite direction. `Window`
turns key presses into a `game::Input` that carries no key codes; systems raise
a `game::GameEvent` that carries no notion of sound, and `main` decides that a
brick makes a noise.

## AI assistance

Claude was used on this project for the following:

- **This README.**
- **The build setup** - `CMakeLists.txt`, `CMakePresets.json`, and the GitHub
  Actions workflow.
- **Refactoring** - suggesting cleaner shapes for parts of the code, most often
  pulling shared arithmetic out of the function it was buried in, which usually
  made it testable as a side effect.
- **Test cases** - deciding what was worth asserting, and drafting many of the
  assertions.
- **Exercising the game** - running builds, the clean-machine check, and
  verifying behaviour.

## References

- [glad](https://glad.dav1d.de/) - the OpenGL loader
- [glfw](https://www.glfw.org/) - the windowing library
- [googletest](https://github.com/google/googletest) - the test framework
- [miniaudio](https://miniaud.io) - audio playback, single header by David Reid
- [learnopengl.com, Audio](https://learnopengl.com/In-Practice/2D-Game/Audio) -
  which sounds a Breakout clone wants and when to play them. **The three sound
  files in `assets/` are the ones from this article.**
- [learnopengl.com](https://learnopengl.com) - shader compilation, vertex array
  and buffer setup, and the batched draw in `platform/Renderer`
- [Handmade Hero](https://handmadehero.org), day 277 - the sparse set entity
  storage in `core/ComponentStore`
- [Microsoft Learn](https://learn.microsoft.com/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew) -
  `GetModuleFileNameW`, and the truncation behaviour the growing buffer in
  `platform/FileSystem` exists to handle
