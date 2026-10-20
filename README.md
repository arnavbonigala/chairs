# chairs

Musical chairs for beans, built on [Skein](../skein).

The world is a storybook meadow: a dirt clearing ringed by rolling hills, groves
of round trees and pines, grass and flowers that sway in the wind, and clouds
drifting overhead. Everything is generated from a seed at startup in about a
second, with no art files. Lighting is cel shaded with ink outlines, rim light
and a painted gradient sky.

The beans are smooth lathed bodies with googly eyes. They hop with squash and
stretch, turn toward where they are going and blink on their own. There are 100
of them, each with a fixed name and colour, so the same bean is always the same
character from one video to the next. Names float above their heads.

## Build

Expects a Skein checkout next to this one, or pass `-DSKEIN_DIR=/path/to/skein`.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/chairs
```

## Controls and flags

| Input | Does |
|---|---|
| drag, scroll | orbit and zoom the camera |
| Space | pause the slow orbit |
| H | frame stats |
| N, `--no-tags` | nametags on or off |
| `--resolution 1920x1080` | render at a fixed size |
| `--record out.mp4 --frames 600` | record a fixed number of frames at 60 fps, then quit |
| `--seed N`, `--beans N` | a different meadow, a different crowd |
| `--lineup` | stand the beans in a row facing the camera |
| `--angle`, `--pitch`, `--distance`, `--still` | place the camera |

## Layout

| File | Contents |
|---|---|
| `src/kit.*` | mesh generators: terrain, trees, pines, bushes, rocks, flowers, grass, clouds, bean body, eye |
| `src/names.hpp` | the 100 bean names, indexed by bean id |
| `src/meadow.*` | the palette, world layout and bean animation |
| `src/main.cpp` | window, camera, look settings, recording |
