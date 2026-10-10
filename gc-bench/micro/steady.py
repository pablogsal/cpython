import gc
import json
import sys
import time


class Obj:
    pass


def make(case, n):
    if case == 'live_lists':
        return [[None] for _ in range(n)]
    if case == 'backward_chain':
        x = None
        for _ in range(n):
            x = [x]
        return x
    if case == 'forward_chain':
        nodes = [[None] for _ in range(n)]
        for i in range(n - 1):
            nodes[i][0] = nodes[i + 1]
        return nodes[0]
    if case == 'items_then_tuple':
        return tuple([[i] for i in range(n)])
    if case == 'items_then_dict':
        items = [[i] for i in range(n)]
        return dict(enumerate(items))
    if case == 'instances':
        out = []
        for i in range(n // 4):
            o = Obj()
            o.a = [i]
            o.b = {'k': (o.a,)}
            out.append(o)
        return out
    if case == 'tree_bottom_up':
        level = [[i] for i in range(n // 2)]
        while len(level) > 1:
            level = [[level[i], level[i + 1]] for i in range(0, len(level) - 1, 2)]
        return level
    raise ValueError(case)


def main(case, n, k):
    gc.disable()
    gc.collect()
    gc.freeze()
    root = make(case, n)
    times = []
    for _ in range(k):
        t = time.process_time_ns()
        gc.collect()
        times.append(time.process_time_ns() - t)
    print(json.dumps({'case': case, 'first_ms': times[0] / 1e6,
                      'steady_min_ms': min(times[1:]) / 1e6}))


if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))
