import sys
from pathlib import Path

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).resolve().parent))
from music import CANCAN, HALL, arrange, notes  # noqa: E402
from supertune import toys  # noqa: E402

OUT = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / "sounds")
ROUNDS = 12
PITCHES = [0.85 + k * 0.5 / 7 for k in range(8)]


def write(name, y, gain=1.0):
    y = np.asarray(y, np.float32) * gain
    if y.ndim == 2:
        y = y.mean(1)
    sf.write(OUT / f"{name}.wav", np.clip(y, -1, 1), toys.SR, subtype="PCM_16")


def loop(song, rnd):
    speed = 1.0 + 0.05 * (rnd - 1)
    _, beats = notes(song["melody"])
    loop_s = beats * 60.0 / (song["bpm"] * speed)
    events = []
    arrange(song, 0.0, 6 * loop_s, speed, events, rnd)
    y = toys.mix(events, 6 * loop_s)
    a, b = int(round(2 * loop_s * toys.SR)), int(round(4 * loop_s * toys.SR))
    return y[a:b]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for rnd in range(1, ROUNDS + 1):
        write(f"cancan-{rnd}", loop(CANCAN, rnd), 0.8)
        write(f"hall-{rnd}", loop(HALL, rnd), 0.8)
    for k, p in enumerate(PITCHES):
        write(f"bonk-{k}", toys.bonk(p, 0.8, seed=k), 0.5)
        big = np.zeros(int(0.6 * toys.SR), np.float32)
        b = toys.bonk(p * 0.8, 1.0, seed=k) * 0.6
        s = toys.slide(1100, 320, 0.45, seed=k) * 0.25
        big[:len(b)] += b
        o = int(0.05 * toys.SR)
        big[o:o + len(s)] += s[:len(big) - o]
        write(f"knock-{k}", big)
        write(f"boing-{k}", toys.boing(p, 0.4, seed=k), 0.12)
        launch = np.zeros(int(2.1 * toys.SR), np.float32)
        w = toys.whoosh(0.9, seed=k) * 0.4
        sc = toys.scream(p, seed=k) * 0.6
        launch[:len(w)] += w
        o = int(0.08 * toys.SR)
        launch[o:o + len(sc)] += sc[:len(launch) - o]
        write(f"launch-{k}", launch)
    for k in range(4):
        write(f"whoosh-{k}", toys.whoosh(0.6, seed=k), 0.25)
        write(f"plop-{k}", toys.plop(0.8, seed=k), 0.45)
    pops = np.zeros(int(0.2 * toys.SR), np.float32)
    for k in range(3):
        p = toys.pop(0.6, seed=k) * 0.3
        o = int(0.04 * k * toys.SR)
        pops[o:o + len(p)] += p
    write("pop", pops)
    write("ding", toys.glockenspiel(88, 0.6), 0.35)
    write("whistle", toys.whistle(0.6), 0.35)
    write("twinkle", toys.twinkle(0.6), 0.3)
    write("fanfare", toys.fanfare(0.8), 0.55)
    print(f"wrote {len(list(OUT.glob('*.wav')))} sounds to {OUT}")


if __name__ == "__main__":
    main()
