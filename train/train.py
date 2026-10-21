import argparse
import json
import struct
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn

from env import Env

ROOT = Path(__file__).resolve().parent.parent
GENS = ROOT / "gens"
CHECKPOINT = Path(__file__).resolve().parent / "checkpoint.pt"


def mlp(inp, out, hidden=256):
    return nn.Sequential(nn.Linear(inp, hidden), nn.Tanh(), nn.Linear(hidden, hidden), nn.Tanh(), nn.Linear(hidden, out))


class Agent(nn.Module):
    def __init__(self, obs, logits, moves):
        super().__init__()
        self.moves = moves
        self.actor = mlp(obs, logits)
        self.critic = mlp(obs, 1)
        nn.init.orthogonal_(self.actor[-1].weight, 0.01)
        nn.init.zeros_(self.actor[-1].bias)

    def heads(self, obs):
        z = self.actor(obs)
        m = self.moves
        return (torch.distributions.Categorical(logits=z[:, :m]), torch.distributions.Categorical(logits=z[:, m:m + 2]),
                torch.distributions.Categorical(logits=z[:, m + 2:m + 4]))

    def act(self, obs):
        heads = self.heads(obs)
        a = torch.stack([h.sample() for h in heads], 1)
        return a, sum(h.log_prob(a[:, i]) for i, h in enumerate(heads)), self.critic(obs).squeeze(1)

    def evaluate(self, obs, a):
        heads = self.heads(obs)
        logp = sum(h.log_prob(a[:, i]) for i, h in enumerate(heads))
        ent = sum(h.entropy() for h in heads)
        return logp, ent, self.critic(obs).squeeze(1)


def export(agent, path):
    layers = [m for m in agent.actor if isinstance(m, nn.Linear)]
    with open(path, "wb") as f:
        f.write(b"CHB1" + struct.pack("<i", len(layers)))
        for l in layers:
            f.write(struct.pack("<ii", l.in_features, l.out_features))
            f.write(l.weight.detach().cpu().numpy().astype("<f4").tobytes())
            f.write(l.bias.detach().cpu().numpy().astype("<f4").tobytes())


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--envs", type=int, default=192)
    p.add_argument("--steps", type=int, default=64)
    p.add_argument("--gen-every", type=int, default=25)
    p.add_argument("--fouls-fill", type=float, default=0.85)
    p.add_argument("--fouls-at", type=int, default=1500)
    p.add_argument("--updates", type=int, default=100000)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--lr", type=float, default=3e-4)
    p.add_argument("--pool-share", type=float, default=0.5)
    p.add_argument("--resume", action="store_true")
    args = p.parse_args()

    torch.manual_seed(args.seed)
    torch.set_num_threads(8)
    env = Env(args.envs, seed=args.seed, fouls=False)
    agent = Agent(env.obs_size, env.logits, env.moves)
    opt = torch.optim.Adam(agent.parameters(), lr=args.lr, eps=1e-5)
    update, samples, gen, fouls = 0, 0, 0, False
    GENS.mkdir(exist_ok=True)
    if args.resume and CHECKPOINT.exists():
        ck = torch.load(CHECKPOINT, weights_only=False)
        agent.load_state_dict(ck["agent"])
        opt.load_state_dict(ck["opt"])
        update, samples, gen, fouls = ck["update"], ck["samples"], ck["gen"], ck["fouls"]
        env = Env(args.envs, seed=args.seed + update, fouls=fouls)

    def open_pool():
        for path in sorted(GENS.glob("gen*.json"), key=lambda q: int(q.stem[3:])):
            if json.loads(path.read_text()).get("fouls"):
                env.add_opponent(path.with_suffix(".bin"))
        env.set_pool(args.pool_share, True)

    if fouls:
        open_pool()
    log = open(ROOT / "train" / "log.csv", "a")
    if log.tell() == 0:
        log.write("update,samples,gen,fouls,games,rounds_per_game,sits_per_round,hits_per_round,knocks_per_round,fouls_per_game,"
                  "seconds_per_game,entropy,value_loss,samples_per_s\n")

    gamma, lam, clip, epochs, minibatches = 0.995, 0.95, 0.2, 4, 8
    T, N = args.steps, env.count * env.agents
    obs_b = torch.zeros(T, N, env.obs_size)
    act_b = torch.zeros(T, N, 3, dtype=torch.long)
    logp_b, val_b, rew_b, done_b, mask_b = (torch.zeros(T, N) for _ in range(5))
    obs, mask = env.observe()
    obs_t, mask_t = torch.from_numpy(obs.copy()), torch.from_numpy(mask.astype(np.float32))

    window = dict.fromkeys(("games", "rounds", "sits", "hits", "knocks", "fouls", "seconds", "finished", "most", "pool_games", "pool_wins"), 0.0)
    since = dict(window)
    next_gen = update + args.gen_every * 2 ** (gen // 20)
    while update < args.updates:
        fill = since["finished"] / since["most"] if since["games"] >= 100 else 0.0
        if not fouls and (update >= args.fouls_at or fill >= args.fouls_fill):
            fouls = True
            env.set_fouls(True)
            open_pool()
            print(f"fouls on at update {update}", flush=True)
        t0 = time.time()
        with torch.no_grad():
            for t in range(T):
                a, logp, v = agent.act(obs_t)
                obs_b[t], act_b[t], logp_b[t], val_b[t], mask_b[t] = obs_t, a, logp, v, mask_t
                obs, rew, done, mask = env.step(a.numpy().astype(np.int32))
                rew_b[t] = torch.from_numpy(rew.copy())
                done_b[t] = torch.from_numpy(done.astype(np.float32))
                obs_t, mask_t = torch.from_numpy(obs.copy()), torch.from_numpy(mask.astype(np.float32))
            next_v = agent.critic(obs_t).squeeze(1)
            adv = torch.zeros(T, N)
            last = torch.zeros(N)
            for t in reversed(range(T)):
                nv = next_v if t == T - 1 else val_b[t + 1]
                live = 1.0 - done_b[t]
                delta = rew_b[t] + gamma * nv * live - val_b[t]
                last = delta + gamma * lam * live * last
                last = last * mask_b[t]
                adv[t] = last
            ret = adv + val_b

        keep = mask_b.reshape(-1) > 0
        b_obs, b_act = obs_b.reshape(-1, env.obs_size)[keep], act_b.reshape(-1, 3)[keep]
        b_logp, b_adv, b_ret = logp_b.reshape(-1)[keep], adv.reshape(-1)[keep], ret.reshape(-1)[keep]
        n = b_obs.shape[0]
        ents, vls = [], []
        for _ in range(epochs):
            perm = torch.randperm(n)
            for idx in perm.chunk(minibatches):
                logp, ent, v = agent.evaluate(b_obs[idx], b_act[idx])
                ratio = (logp - b_logp[idx]).exp()
                a_ = b_adv[idx]
                a_ = (a_ - a_.mean()) / (a_.std() + 1e-8)
                pg = -torch.min(ratio * a_, ratio.clamp(1 - clip, 1 + clip) * a_).mean()
                vl = 0.5 * (v - b_ret[idx]).pow(2).mean()
                loss = pg + 0.5 * vl - 0.01 * ent.mean()
                opt.zero_grad()
                loss.backward()
                nn.utils.clip_grad_norm_(agent.parameters(), 0.5)
                opt.step()
                ents.append(ent.mean().item())
                vls.append(vl.item())
        update += 1
        samples += n
        rate = n / (time.time() - t0)
        for k, v in env.stats().items():
            window[k] += v
            since[k] += v
        if update % 10 == 0:
            s = window
            g, r = max(s["games"], 1), max(s["rounds"], 1)
            log.write(f"{update},{samples},{gen},{int(fouls)},{s['games']:.0f},{s['finished'] / g:.3f},{s['sits'] / r:.3f},{s['hits'] / r:.3f},"
                      f"{s['knocks'] / r:.3f},{s['fouls'] / g:.3f},{s['seconds'] / g:.2f},{np.mean(ents):.4f},{np.mean(vls):.4f},{rate:.0f}\n")
            log.flush()
            window = dict.fromkeys(window, 0.0)
            print(f"update {update} fill {s['finished'] / max(s['most'], 1):.2f} samples {samples / 1e6:.1f}M fouls {int(fouls)} rounds/game {s['finished'] / g:.2f} "
                  f"sits/round {s['sits'] / r:.2f} knocks/round {s['knocks'] / r:.2f} fouls/game {s['fouls'] / g:.2f} "
                  f"pool wins {s['pool_wins'] / max(s['pool_games'], 1):.2f} entropy {np.mean(ents):.3f} {rate / 1e3:.0f}k/s", flush=True)
        if update >= next_gen:
            s = since
            g, r = max(s["games"], 1), max(s["rounds"], 1)
            since = dict.fromkeys(since, 0.0)
            gen += 1
            next_gen = update + args.gen_every * 2 ** (gen // 20)
            export(agent, GENS / f"gen{gen}.bin")
            (GENS / f"gen{gen}.json").write_text(json.dumps({
                "gen": gen, "update": update, "samples": samples, "fouls": fouls,
                "rounds_per_game": round(s["finished"] / g, 3), "fill": round(s["finished"] / max(s["most"], 1), 3), "sits_per_round": round(s["sits"] / r, 3),
                "knocks_per_round": round(s["knocks"] / r, 3), "pool_win_rate": round(s["pool_wins"] / max(s["pool_games"], 1), 3), "fouls_per_game": round(s["fouls"] / g, 3),
                "saved": time.strftime("%Y-%m-%d %H:%M:%S")}, indent=1) + "\n")
            if fouls:
                env.add_opponent(GENS / f"gen{gen}.bin")
            torch.save({"agent": agent.state_dict(), "opt": opt.state_dict(), "update": update, "samples": samples,
                        "gen": gen, "fouls": fouls}, CHECKPOINT)
            print(f"saved gen{gen}", flush=True)


if __name__ == "__main__":
    main()
