import json
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GENS = ROOT / "gens"
BEST = GENS / "best.json"
SIM = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parent / "chairs_sim")
GAMES, SPLIT, CROWN = 300, 4, 0.55


def match(a, b):
    def run(k):
        out = subprocess.run([str(SIM), "--gen", str(a), "--vs", str(b), "--players", "10", "--games", str(GAMES // SPLIT),
                              "--seed", str(9000 + 1000 * k + a), "-q"], cwd=ROOT, capture_output=True, text=True).stdout
        m = re.search(r"won (\d+), gen -?\d+ won (\d+)", out)
        return (int(m.group(1)), int(m.group(2))) if m else (0, 0)
    with ThreadPoolExecutor(SPLIT) as ex:
        results = list(ex.map(run, range(SPLIT)))
    return sum(r[0] for r in results), sum(r[1] for r in results)


def main():
    state = json.loads(BEST.read_text()) if BEST.exists() else {"gen": 67, "judged": 67, "history": [{"gen": 67, "note": "round-robin"}]}
    BEST.write_text(json.dumps(state, indent=1) + "\n")
    while True:
        gens = sorted(int(p.stem[3:]) for p in GENS.glob("gen*.json"))
        pending = [g for g in gens if g > state["judged"] and json.loads((GENS / f"gen{g}.json").read_text()).get("fouls")]
        if not pending:
            time.sleep(60)
            continue
        g = pending[0]
        wins, champ = match(g, state["gen"])
        share = wins / max(wins + champ, 1)
        bots, botsl = match(g, -1)
        meta = json.loads((GENS / f"gen{g}.json").read_text())
        meta["vs_best"] = {"best": state["gen"], "wins": wins, "losses": champ}
        meta["vs_bots"] = {"wins": bots, "losses": botsl}
        (GENS / f"gen{g}.json").write_text(json.dumps(meta, indent=1) + "\n")
        crowned = share >= CROWN
        line = f"gen {g} vs best gen {state['gen']}: {wins}-{champ} ({share:.0%}), vs bots {bots}-{botsl}" + (" NEW BEST" if crowned else "")
        print(time.strftime("%H:%M:%S"), line, flush=True)
        if crowned:
            state["gen"] = g
            state["history"].append({"gen": g, "beat": state["history"][-1]["gen"], "share": round(share, 3)})
        state["judged"] = g
        BEST.write_text(json.dumps(state, indent=1) + "\n")


if __name__ == "__main__":
    main()
