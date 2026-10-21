import ctypes
import os
from pathlib import Path

import numpy as np

LIB = Path(os.environ.get("CHAIRS_ENV", Path(__file__).resolve().parent.parent / "build" / "libchairs_env.dylib"))


class Env:
    def __init__(self, count, min_players=2, max_players=10, seed=1, fouls=True, threads=8):
        lib = ctypes.CDLL(str(LIB))
        lib.chairs_env_new.restype = ctypes.c_void_p
        lib.chairs_env_new.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_int, ctypes.c_int]
        for name in ("chairs_env_free", "chairs_env_observe", "chairs_env_step", "chairs_env_stats", "chairs_env_set_fouls"):
            getattr(lib, name).restype = None
        lib.chairs_env_free.argtypes = [ctypes.c_void_p]
        lib.chairs_env_set_fouls.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.chairs_env_add_opponent.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        lib.chairs_env_set_pool.restype = None
        lib.chairs_env_clear_pool.restype = None
        lib.chairs_env_set_rules.restype = None
        lib.chairs_env_set_bot_share.restype = None
        lib.chairs_env_set_bot_share.argtypes = [ctypes.c_void_p, ctypes.c_float]
        lib.chairs_env_set_rules.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.chairs_env_clear_pool.argtypes = [ctypes.c_void_p]
        lib.chairs_env_set_pool.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_int]
        self.lib = lib
        self.obs_size = lib.chairs_obs_size()
        self.logits = lib.chairs_logits()
        self.moves = lib.chairs_moves()
        self.count, self.agents = count, max_players
        self.n = count * max_players
        self.handle = ctypes.c_void_p(lib.chairs_env_new(count, min_players, max_players, seed, int(fouls), threads))
        n = count * max_players
        self.obs = np.zeros((n, self.obs_size), np.float32)
        self.mask = np.zeros(n, np.uint8)
        self.reward = np.zeros(n, np.float32)
        self.done = np.zeros(n, np.uint8)
        self.actions = np.zeros((n, 3), np.int32)

    def _p(self, a):
        return a.ctypes.data_as(ctypes.c_void_p)

    def observe(self):
        self.lib.chairs_env_observe(self.handle, self._p(self.obs), self._p(self.mask))
        return self.obs, self.mask

    def step(self, actions):
        self.actions[:] = actions
        self.lib.chairs_env_step(self.handle, self._p(self.actions), self._p(self.obs), self._p(self.reward),
                                 self._p(self.done), self._p(self.mask))
        return self.obs, self.reward, self.done, self.mask

    def set_fouls(self, on):
        self.lib.chairs_env_set_fouls(self.handle, int(on))

    def stats(self):
        out = np.zeros(11, np.float64)
        self.lib.chairs_env_stats(self.handle, self._p(out))
        return dict(zip(("games", "rounds", "sits", "hits", "knocks", "fouls", "seconds", "finished", "most", "pool_games", "pool_wins"), out))

    def add_opponent(self, path):
        return bool(self.lib.chairs_env_add_opponent(self.handle, str(path).encode()))

    def set_bot_share(self, share):
        self.lib.chairs_env_set_bot_share(self.handle, share)

    def set_rules(self, version):
        self.lib.chairs_env_set_rules(self.handle, version)

    def clear_pool(self):
        self.lib.chairs_env_clear_pool(self.handle)

    def set_pool(self, share, bots=True):
        self.lib.chairs_env_set_pool(self.handle, share, int(bots))

    def __del__(self):
        if getattr(self, "handle", None):
            self.lib.chairs_env_free(self.handle)


class Group:
    def __init__(self, envs):
        self.envs = envs
        self.obs_size, self.logits, self.moves = envs[0].obs_size, envs[0].logits, envs[0].moves
        self.n = sum(e.n for e in envs)
        self.cuts = np.cumsum([0] + [e.n for e in envs])

    def _join(self, parts):
        return [np.concatenate(p) for p in zip(*parts)]

    def observe(self):
        return self._join([e.observe() for e in self.envs])

    def step(self, actions):
        return self._join([e.step(actions[a:b]) for e, a, b in zip(self.envs, self.cuts[:-1], self.cuts[1:])])

    def stats(self):
        out = {}
        for e in self.envs:
            for k, v in e.stats().items():
                out[k] = out.get(k, 0.0) + v
        return out

    def __getattr__(self, name):
        def each(*args):
            return [getattr(e, name)(*args) for e in self.envs][0]
        return each
