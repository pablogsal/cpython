import argparse
import json
import os
import shutil
import statistics
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
CASES = ['sphinx', 'pylint', 'parso', 'sqlglot', 'bs4', 'networkx', 'black',
         'sympy', 'mypy', 'mypy_hosted']
p = argparse.ArgumentParser()
p.add_argument('baseline', help='unmodified python executable')
p.add_argument('candidate', help='patched python executable')
p.add_argument('--cpython', required=True, type=Path,
               help='CPython checkout: its Doc/ is built by Sphinx, its Lib/ is linted')
p.add_argument('--mypy-src', type=Path, help='mypy checkout, for the mypy cases')
p.add_argument('--site', required=True,
               help='directory with the packages from requirements.txt, built for these executables')
p.add_argument('--cpu', required=True)
p.add_argument('--blocks', type=int, default=2)
p.add_argument('--out', type=Path, default=HERE / 'results')
p.add_argument('--cases', nargs='+', default=CASES, choices=CASES)
a = p.parse_args()
exes = {'baseline': a.baseline, 'candidate': a.candidate}
env = dict(os.environ, PYTHONHASHSEED='0', LC_ALL='C.UTF-8', PYTHONPATH=a.site)
rows = []


def command(case, directory):
    if case == 'sphinx':
        return a.cpython, ['sphinx', '-b', 'html', '-j', '1', '-E', '-a', '-q',
                           '-d', str(directory / 'doctrees'), 'Doc', str(directory / 'html')]
    if case in ('mypy', 'mypy_hosted'):
        args = ['mypy', '--config-file', 'mypy_self_check.ini', '--num-workers', '0',
                '--no-incremental', '--cache-dir', str(directory / 'cache'),
                '-p', 'mypy', '-p', 'mypyc']
        return a.mypy_src, args + (['--no-fast-exit'] if case == 'mypy_hosted' else [])
    if case == 'pylint':
        return a.cpython, ['pylint', '--rcfile=/dev/null', '-j', '1', '--exit-zero',
                           '--score=n', '--reports=n', 'Lib/json', 'Lib/email',
                           'Lib/logging', 'Lib/asyncio', 'Lib/argparse.py']
    if case == 'black':
        return a.cpython, ['black', '--check', '-q', '--workers', '1',
                           'Lib/json', 'Lib/email', 'Lib/logging']
    return HERE, [f'{case}_workload']


for case in a.cases:
    for index, who in enumerate(('baseline', 'candidate', 'candidate', 'baseline') * a.blocks):
        directory = a.out / f'{case}-{index}-{who}'
        shutil.rmtree(directory, ignore_errors=True)
        directory.mkdir(parents=True)
        cwd, args = command(case, directory)
        result = directory / 'result.json'
        with (directory / 'log.txt').open('w') as log:
            subprocess.run(['taskset', '-c', a.cpu, 'setarch', 'x86_64', '-R', exes[who],
                            str(HERE / 'worker.py'), '--output', str(result), *args],
                           cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT)
        row = json.loads(result.read_text())
        row.update(case=case, who=who, index=index)
        for name in ('html', 'doctrees', 'cache'):
            shutil.rmtree(directory / name, ignore_errors=True)
        rows.append(row)
        (a.out / 'runs.json').write_text(json.dumps(rows, indent=1) + '\n')
        gc_s = sum(g['duration'] for g in row['gc'])
        print(f'{case} {who} {index}: status={row["status"]} cpu={row["cpu_s"]:.2f}s gc={gc_s:.2f}s',
              flush=True)
    for who in exes:
        sel = [r for r in rows if r['case'] == case and r['who'] == who]
        print(f'== {case} {who}: median cpu={statistics.median(r["cpu_s"] for r in sel):.3f}s '
              f'gc={statistics.median(sum(g["duration"] for g in r["gc"]) for r in sel):.3f}s',
              flush=True)
