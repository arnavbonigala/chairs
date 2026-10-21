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
character from one video to the next. Names float above their heads and fade out
when another bean blocks the view. Standing in formation, they breathe, glance
around and do the odd hop in place.

The game is classic musical chairs with contact. Beans walk the ring while the
music plays, and it stops at a random moment with no warning. Then it is a
scramble: one chair short, and anyone who is not fully seated can be shoved off.
A bean locks in after sitting still for a moment. Whoever is left standing gets
launched into the sky. The game runs on real physics at 240 Hz and is
deterministic, so the same seed always plays out the same way.

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
| `--grid 10` | stand them in ranks of 10, the contestant shot with `--beans 100` |
| `--angle`, `--pitch`, `--distance`, `--still` | place the camera |
| `--shot NAME` | fly a keyframed camera move and quit when it ends: flyover, reveal, dolly, orbit, rows, closeup-kevin, closeup-lilmoist, closeup-wobbles |
| `--warmup S` | run the world S seconds before the first frame |
| `--scene round`, `--scene final` | a scripted round with six beans, or the two-bean final |
| `--game --players N` | a full physics game with N bot players |
| `--cast "Kevin,Gus"` | pick the players by name |
| `--shot wide\|top\|low\|chairs\|follow\|action` | game cameras; `--focus Kevin` sets who `follow` tracks |
| `--rounds 1,5` | record only those rounds |
| `--gen 0` | generation 0: untrained players that mash random inputs, with fouls off |
| `--gen N` | players run trained generation N from `gens/` (set `CHAIRS_GENS` to look elsewhere) |
| `--replay take` | render a saved take, use with `--record` and `--resolution` |
| `--empty N` | N chairs and no beans, for the empty-chair shots (`--shot empty-wide`, `empty-push`, `empty-orbit`, `empty-crane`, `empty-chair`) |

Running a game without `--shot` or `--record` puts you behind a free camera:
WASD to move, Q and E for down and up, the mouse to look (Tab frees the cursor),
scroll to zoom, Shift to go fast and Alt to go slow. The game holds before each
round until you press Enter, and Space freezes everything but the camera. R
starts and stops a take, saved to `takes/` with your camera path and the game's
timing. `tools/render-takes.sh` renders every new take in 4K with music and
sound. Press ? for the controls on screen.

The live game plays its music and sound effects too, from a sound bank of WAVs
made with the same synth. Build it once with
`uv run --project ../supertune python tools/soundbank.py`, which writes
`sounds/`. Audio goes through [miniaudio](https://miniaud.io), vendored in
`third_party/`.

Game recordings also write `out.cues` with when the music plays and stops and every game event.
`uv run --project ../supertune python tools/music.py out.mp4` scores the clip
with toy-instrument versions of the Can-can and In the Hall of the Mountain
King, cut on the real stops, adds sound effects for shoves, hits, sits, launches and the win, and saves the track as `out.wav`. It needs a
supertune checkout next to this one.

## Training

The beans learn by self-play with PPO. `chairs_env` is a shared library that
steps many games at once across every core, and `train/train.py` drives it from
PyTorch. Observations and actions live in `src/brain.*`, so the trainer and the
app always agree on them. Every bean sees itself, its 8 nearest rivals and its 6
nearest chairs in a frame that turns with it around the ring. It picks a move
direction, whether to jump and whether to shove, 30 times a second. Surviving a
round is worth +1, elimination -1 and the win +2.

Half the games mix in frozen opponents: an older gen from the fouls era or the
scripted bots, so a gen has to beat every style it has seen and not just itself.

Training starts with fouls off and turns them on once the beans fill most of
the chairs. Each saved generation goes to `gens/genN.bin` with a `genN.json`
next to it recording its rules and stats. Gens come every 25 updates at first,
and the gap doubles every 20 gens.

`train/judge.py` plays every new gen against the current champion over 300
ten-bean games, and a gen takes the crown with 55% of the wins. The champion is
kept in `gens/best.json` and the app opens it with `--gen best`. The pool of
frozen opponents is the champion, the 12 newest gens and the scripted bots.

```
cmake --build build -j
cd train && uv run python train.py            # --resume picks up from train/checkpoint.pt
./build/chairs_sim --gen 12 --vs 4 --players 10 --games 500 -q
```

`chairs_sim` plays games headless at about half a million steps a second and
prints who got hit and knocked off each round: `./build/chairs_sim --seed 56 -v`.

## Layout

| File | Contents |
|---|---|
| `src/kit.*` | mesh generators: terrain, trees, pines, bushes, rocks, flowers, grass, clouds, bean body, eye |
| `src/names.hpp` | the 100 bean names, indexed by bean id |
| `src/meadow.*` | the palette, world layout and bean animation |
| `src/shots.*` | keyframed camera moves on Catmull-Rom splines |
| `src/stage.*` | scripted round and final scenes |
| `src/game.*` | the rules and physics of musical chairs |
| `src/bots.*` | scripted bot players |
| `src/view.*` | draws a game in the meadow, with its cameras |
| `src/fx.*` | confetti, sparks, launch trails |
| `tools/sim.cpp` | headless game runner |
| `tools/music.py` | scores game recordings from their cues |
| `tools/render-takes.sh` | renders saved takes in 4K |
| `tools/soundbank.py` | renders the music loops and effects the live game plays |
| `src/audio.*` | live music and sound effects |
| `src/take.*` | saving, loading and sampling camera takes |
| `src/brain.*` | observations, actions and the policy network for trained gens |
| `src/env.cpp` | the batched training environment, as a C library |
| `train/` | the PPO trainer |
| `src/main.cpp` | window, camera, look settings, recording |
