import argparse
import json
import statistics
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
p = argparse.ArgumentParser()
p.add_argument('exes', nargs='+', help='label=python; the first one is the reference')
p.add_argument('--cpu', required=True)
p.add_argument('--rounds', type=int, default=9)
a = p.parse_args()
exes = [e.split('=', 1) for e in a.exes]
for name, extra in (('ast', []), ('ast_parents', ['--parents'])):
    keys = ('grow_gc', 'full', 'churn_gc')
    samples = {k: {label: [] for label, _ in exes} for k in keys}
    for r in range(a.rounds):
        for label, exe in (exes if r % 2 == 0 else exes[::-1]):
            d = json.loads(subprocess.check_output(
                ['taskset', '-c', a.cpu, 'setarch', 'x86_64', '-R', exe,
                 str(HERE / 'realheap.py'), *extra], text=True))
            samples['grow_gc'][label].append(d['grow']['gc_seconds'])
            samples['full'][label].append(d['full']['min_seconds'])
            samples['churn_gc'][label].append(d['churn']['gc_seconds'])
    for k in keys:
        ref = None
        for label, _ in exes:
            med, low = statistics.median(samples[k][label]), min(samples[k][label])
            ref = ref or med
            print(f'{name}:{k:9} {label:10} median {med * 1e3:8.1f} ms  min {low * 1e3:8.1f} ms  '
                  f'{(1 - med / ref) * 100:+6.1f}%', flush=True)
