import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

from supertune import toys

LETTER = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def midi(name):
    pc = LETTER[name[0]] + name[1:-1].count("#") - name[1:-1].count("b")
    return 12 * (int(name[-1]) + 1) + pc


def notes(text):
    out, beat = [], 0.0
    for tok in text.split():
        name, dur = tok.split(":")
        if name != "r":
            out.append((beat, float(dur), midi(name)))
        beat += float(dur)
    return out, beat


CANCAN_A = "C5:2 D5:.5 F5:.5 E5:.5 D5:.5 G5:1 G5:1 G5:.5 A5:.5 E5:.5 F5:.5 D5:1 D5:1 D5:.5 F5:.5 E5:.5 D5:.5"
CANCAN = {
    "bpm": 264,
    "melody": " ".join([CANCAN_A, "C5:.5 C6:.5 B5:.5 A5:.5 G5:.5 F5:.5 E5:.5 D5:.5",
                        CANCAN_A, "C5:.5 G5:.5 D5:.5 E5:.5 C5:1 r:1"]),
    "bass": ["C3 G2", "C3 G2", "G2 D3", "G2 D3", "C3 G2", "C3 G2", "G2 D3", "G2 C3"],
    "chords": ["C4 E4 G4", "C4 E4 G4", "B3 D4 F4", "B3 D4 F4", "C4 E4 G4", "C4 E4 G4", "B3 D4 F4", "B3 D4 F4|C4 E4 G4"],
}

HALL_A = "B4:.5 C#5:.5 D5:.5 E5:.5 F#5:.5 D5:.5 F#5:1 F5:.5 C#5:.5 F5:1 E5:.5 C5:.5 E5:1"
HALL_B = "B4:.5 C#5:.5 D5:.5 E5:.5 F#5:.5 D5:.5 F#5:.5 B5:.5 A5:.5 F#5:.5 D5:.5 F#5:.5 A5:2"
HALL = {
    "bpm": 220,
    "melody": " ".join([HALL_A, HALL_B]),
    "bass": ["B2 F#2 B2 F#2", "C#3 C#3 C3 C3", "B2 F#2 B2 F#2", "D3 A2 D3 r"],
    "chords": None,
}


def arrange(song, start, length, speed, events, seed):
    bpm = song["bpm"] * speed
    beat_s = 60.0 / bpm
    mel, loop_beats = notes(song["melody"])
    loop_s = loop_beats * beat_s
    rng = np.random.default_rng(seed)
    reps = int(length / loop_s) + 2
    for k in range(reps):
        base = start + k * loop_s
        up = 12 if k % 2 else 0
        for b, _, m in mel:
            t = base + b * beat_s
            if t - start >= length:
                break
            v = rng.uniform(0.75, 0.9)
            events.append((t, toys.toy_piano(m + up, v, seed=len(events)), 0.55, 0.0))
            events.append((t, toys.glockenspiel(m + 12, v, seed=len(events)), 0.2, 0.35))
        for bar, line in enumerate(song["bass"]):
            hits = line.split()
            step = 4 / len(hits) if song["chords"] is None else 2
            for i, name in enumerate(hits):
                if name != "r":
                    events.append((base + (bar * 4 + i * step) * beat_s, toys.marimba(midi(name), 0.9, seed=len(events)), 0.7, -0.25))
            if song["chords"]:
                parts = song["chords"][bar].split("|")
                for i, off in enumerate((1, 3)):
                    chord = parts[min(i, len(parts) - 1)]
                    for name in chord.split():
                        events.append((base + (bar * 4 + off) * beat_s, toys.toy_piano(midi(name), 0.45, seed=len(events)), 0.25, -0.1))
            for i in range(8):
                t = base + (bar * 4 + i * 0.5) * beat_s
                if i % 2:
                    events.append((t, toys.shaker(0.5, seed=len(events)), 0.25, 0.4))
                if i in (2, 6):
                    events.append((t, toys.woodblock(0.7, seed=len(events)), 0.35, 0.2))


def cues(path):
    rows = [line.split() for line in Path(path).read_text().splitlines()]
    end = float(rows[-1][1])
    spans = []
    for i, r in enumerate(rows):
        if r[0] == "play":
            stop = next(float(s[1]) for s in rows[i + 1:] if s[0] in ("stop", "end"))
            spans.append((float(r[1]), stop, float(r[2]), int(r[3]), int(r[4])))
    return spans, end, rows


def _voice(i):
    return 0.85 + (i * 0.618 % 1) * 0.5, (i * 0.37 % 1) * 1.2 - 0.6


def effects(rows):
    events, last = [], {}
    launched = [(float(r[1]), int(r[3])) for r in rows if r[0] == "ev" and r[2] == "8"]
    for n, r in enumerate(rows):
        if r[0] not in ("ev", "jump", "twinkle"):
            continue
        t = float(r[1])
        kind = r[2] if r[0] == "ev" else r[0]
        who = int(r[3] if r[0] == "ev" else r[2])
        pitch, pan = _voice(who)
        if kind in ("jump", "2") and t - last.get((kind, who), -1) < 0.25:
            continue
        last[(kind, who)] = t
        if kind == "jump":
            events.append((t, toys.boing(pitch, 0.4, seed=n), 0.12, pan))
        elif kind == "twinkle":
            events.append((t, toys.twinkle(0.6, seed=n), 0.3, pan))
        elif kind == "0" and t > 0.5:
            for k in range(3):
                events.append((t + 0.04 * k, toys.pop(0.6, seed=n + k), 0.3, (k - 1) * 0.4))
        elif kind == "2":
            events.append((t, toys.whoosh(0.6, seed=n), 0.25, pan))
        elif kind == "3":
            events.append((t, toys.bonk(pitch, 0.8, seed=n), 0.5, pan))
        elif kind == "4":
            events.append((t, toys.plop(0.8, seed=n), 0.45, pan))
        elif kind == "5":
            events.append((t, toys.bonk(pitch * 0.8, 1.0, seed=n), 0.6, pan))
            events.append((t + 0.05, toys.slide(1100, 320, 0.45, seed=n), 0.25, pan))
        elif kind == "6":
            events.append((t, toys.glockenspiel(88, 0.6, seed=n), 0.35, pan))
        elif kind == "7":
            events.append((t, toys.whistle(0.6, seed=n), 0.35, 0.0))
        elif kind == "8":
            crowd = sum(abs(a - t) < 0.05 for a, _ in launched)
            events.append((t, toys.whoosh(0.9, seed=n), 0.4, pan))
            events.append((t + 0.08, toys.scream(pitch, seed=n), 0.6 / np.sqrt(crowd), pan))
        elif kind == "10":
            events.append((t, toys.fanfare(0.8, seed=n), 0.55, 0.0))
    return events


def render(video):
    video = Path(video)
    spans, end, rows = cues(video.with_suffix(".cues"))
    out = toys.mix(effects(rows), end)
    for n, (a, b, offset, rnd, alive) in enumerate(spans):
        song = HALL if alive <= 2 else CANCAN
        events = []
        arrange(song, a - offset, b - a + offset, 1.0 + 0.05 * (rnd - 1), events, n)
        seg = toys.mix(events, end)
        i, j = int(a * toys.SR), int(b * toys.SR)
        fade = int(0.006 * toys.SR)
        seg[:i] = 0
        seg[j:] = 0
        seg[j - fade:j] *= np.linspace(1, 0, fade)[:, None]
        out += seg
    wav = video.with_suffix(".wav")
    sf.write(wav, toys.master(out), toys.SR, subtype="PCM_24")
    tmp = video.with_name(video.stem + ".tmp.mp4")
    subprocess.run(["ffmpeg", "-y", "-v", "error", "-i", str(video), "-i", str(wav), "-map", "0:v", "-map", "1:a",
                    "-c:v", "copy", "-c:a", "aac", "-b:a", "320k", str(tmp)], check=True)
    tmp.replace(video)
    print(f"{video}: {len(spans)} music spans, {wav.name}")


if __name__ == "__main__":
    for v in sys.argv[1:]:
        render(v)
