import argparse
import json
import pathlib
import statistics
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
CASES = ['live_lists', 'live_dicts', 'atomic_tuples', 'nested_tuples',
         'forward_chain', 'backward_chain', 'dead_lists', 'dead_dicts', 'dead_nodes',
         'mixed', 'finalizers', 'resurrection', 'weakrefs', 'weakref_callbacks',
         'gen0_live', 'gen1_live', 'auto_cycles', 'auto_tree']

p = argparse.ArgumentParser()
p.add_argument('baseline')
p.add_argument('candidate')
p.add_argument('output')
p.add_argument('--cpu', default='8')
p.add_argument('--no-aslr', action='store_true')
p.add_argument('--blocks', type=int, default=3)
p.add_argument('--samples', type=int, default=7)
p.add_argument('--size', type=int, default=100000)
p.add_argument('--cases', nargs='+', default=CASES)
a = p.parse_args()
rows = []
for case in a.cases:
    for block in range(a.blocks):
        for label in ('baseline', 'candidate', 'candidate', 'baseline'):
            exe = getattr(a, label)
            launcher = ['taskset', '-c', a.cpu]
            if a.no_aslr:
                launcher += ['setarch', 'x86_64', '-R']
            command = [*launcher, exe, str(HERE / 'bench_gc.py'), case,
                       '--size', str(a.size), '--samples', str(a.samples)]
            result = json.loads(subprocess.check_output(command, text=True))
            result.update(label=label, block=block, executable=exe, cpu=a.cpu, no_aslr=a.no_aslr)
            rows.append(result)
            pathlib.Path(a.output).write_text(json.dumps(rows, indent=2) + '\n')
    base = [r['median_cpu_ns'] for r in rows if r['case'] == case and r['label'] == 'baseline']
    cand = [r['median_cpu_ns'] for r in rows if r['case'] == case and r['label'] == 'candidate']
    b, c = statistics.median(base), statistics.median(cand)
    print(f'{case:20} {b / 1e6:9.3f} -> {c / 1e6:9.3f} ms  {b/c:5.3f}x  {(1-c/b)*100:+6.2f}%', flush=True)
