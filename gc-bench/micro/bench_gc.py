import argparse
import gc
import json
import statistics
import sys
import time
import weakref

class Node:
    __slots__ = ('link', '__weakref__')

class FinalNode(Node):
    def __del__(self):
        pass

resurrected = []
class ResurrectNode(Node):
    def __del__(self):
        resurrected.append(self)


def make_graph(case, n):
    if case in ('live_lists', 'gen0_live', 'gen1_live', 'mixed'):
        root = [[None] for _ in range(n)]
        if case == 'mixed':
            for _ in range(n // 10):
                x = []
                x.append(x)
            del x
        return root
    if case == 'live_dicts':
        return [{'value': i} for i in range(n)]
    if case == 'atomic_tuples':
        return [(i, i + 1) for i in range(n)]
    if case == 'nested_tuples':
        return [((i,),) for i in range(n)]
    if case in ('forward_chain', 'backward_chain'):
        if case == 'forward_chain':
            nodes = [[None] for _ in range(n)]
            for i in range(n - 1):
                nodes[i][0] = nodes[i + 1]
            return nodes[0]
        x = None
        for _ in range(n):
            x = [x]
        return x
    if case == 'dead_lists':
        for _ in range(n):
            x = []
            x.append(x)
        return None
    if case == 'dead_dicts':
        for _ in range(n):
            x = {}
            x['self'] = x
        return None
    if case in ('dead_nodes', 'finalizers', 'resurrection', 'weakrefs', 'weakref_callbacks'):
        cls = FinalNode if case == 'finalizers' else ResurrectNode if case == 'resurrection' else Node
        refs = [] if case in ('weakrefs', 'weakref_callbacks') else None
        callback = (lambda ref: None) if case == 'weakref_callbacks' else None
        for _ in range(n):
            x = cls()
            x.link = x
            if refs is not None:
                refs.append(weakref.ref(x, callback))
        return refs
    raise ValueError(case)


def run(case, n, samples):
    gc.disable()
    gc.collect()
    gc.freeze()
    rows = []
    for _ in range(samples + 2):
        gc.collect()
        if case in ('auto_cycles', 'auto_tree'):
            gc.enable()
            start_wall = time.perf_counter_ns()
            start_cpu = time.process_time_ns()
            if case == 'auto_cycles':
                for i in range(n):
                    x = [i]
                    x.append(x)
                del x
            else:
                for i in range(n // 100):
                    x = [{'value': j, 'items': [j, j + 1]} for j in range(100)]
                del x
            cpu = time.process_time_ns() - start_cpu
            wall = time.perf_counter_ns() - start_wall
            gc.disable()
            count = None
        else:
            root = make_graph(case, n)
            generation = 0 if case == 'gen0_live' else 1 if case == 'gen1_live' else 2
            start_wall = time.perf_counter_ns()
            start_cpu = time.process_time_ns()
            count = gc.collect(generation)
            cpu = time.process_time_ns() - start_cpu
            wall = time.perf_counter_ns() - start_wall
            del root
            resurrected.clear()
        rows.append({'cpu_ns': cpu, 'wall_ns': wall, 'collected': count})
    return {'case': case, 'n': n, 'samples': rows[2:],
            'median_cpu_ns': statistics.median(x['cpu_ns'] for x in rows[2:]),
            'median_wall_ns': statistics.median(x['wall_ns'] for x in rows[2:]),
            'version': sys.version, 'gil': sys._is_gil_enabled()}

if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('case')
    p.add_argument('--size', type=int, default=100000)
    p.add_argument('--samples', type=int, default=7)
    args = p.parse_args()
    print(json.dumps(run(args.case, args.size, args.samples)))
