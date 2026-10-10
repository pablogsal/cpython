import argparse
import ast
import gc
import json
import os
import pathlib
import sys
import time

SOURCES = pathlib.Path(os.__file__).parent


def snapshot():
    return [(s['collections'], s['candidates'], s['collected'], s['duration'])
            for s in gc.get_stats()]


def delta(before, after):
    gens = [{'collections': a[0] - b[0], 'candidates': a[1] - b[1],
             'collected': a[2] - b[2], 'duration': a[3] - b[3]}
            for b, a in zip(before, after)]
    return {'gc_seconds': sum(g['duration'] for g in gens), 'generations': gens}


def add_parents(tree):
    for node in ast.walk(tree):
        for child in ast.iter_child_nodes(node):
            child.parent = node


def parse_all(texts, parents):
    trees = []
    for text in texts:
        tree = ast.parse(text)
        if parents:
            add_parents(tree)
        trees.append(tree)
    return trees


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--files', type=int, default=120)
    p.add_argument('--parents', action='store_true')
    p.add_argument('--full', type=int, default=5)
    a = p.parse_args()
    paths = sorted(SOURCES.glob('*.py'))[:a.files]
    texts = [path.read_text(encoding='utf-8') for path in paths]
    out = {'parents': a.parents, 'files': len(texts), 'version': sys.version}

    gc.collect()
    before, cpu = snapshot(), time.process_time()
    live = parse_all(texts, a.parents)
    out['grow'] = delta(before, snapshot())
    out['grow']['cpu_seconds'] = time.process_time() - cpu

    gc.collect()
    out['tracked'] = len(gc.get_objects())
    samples = []
    for _ in range(a.full):
        start = time.perf_counter()
        gc.collect()
        samples.append(time.perf_counter() - start)
    out['full'] = {'min_seconds': min(samples), 'samples': samples}

    before, cpu = snapshot(), time.process_time()
    for _ in range(2):
        parse_all(texts[:a.files // 2], a.parents)
    out['churn'] = delta(before, snapshot())
    out['churn']['cpu_seconds'] = time.process_time() - cpu
    del live
    print(json.dumps(out))


if __name__ == '__main__':
    main()
