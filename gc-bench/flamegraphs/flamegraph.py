import collections
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FG = Path(sys.argv[1])
TRIVIAL = re.compile(r'^(_PyGCHead_\w+|_Py_FROM_GC|_Py_AS_GC|_Py_IsImmortal|_Py_IsStaticImmortal|'
                     r'_PyObject_IS_GC|_PyObject_GC_IS_TRACKED|_PyObject_GC_UNTRACK|_PyObject_GC_MAY_BE_TRACKED|'
                     r'Py_TYPE|_Py_TYPE|Py_SIZE|_Py_SIZE|Py_REFCNT|_Py_REFCNT|Py_IS_TYPE|_Py_IS_TYPE|'
                     r'PyType_HasFeature|PyType_Check|PyObject_IS_GC|_PyType_IS_GC|PyTuple_CheckExact|'
                     r'PyTuple_GET_SIZE|PyTuple_GET_ITEM|_PyObject_CAST|'
                     r'gc_get_refs|gc_set_refs|gc_reset_refs|gc_decref|gc_is_collecting|gc_clear_collecting|'
                     r'gc_list_init|gc_list_is_empty|gc_list_append|gc_list_remove)$')
LABELS = {'base': 'main', 'fold': 'PR 159154', 'order': 'PR 159152'}
WORK = {'ast': 'stdlib ASTs, 720k tracked objects', 'pylint': 'pylint on five stdlib packages'}


def fold(name):
    folded = HERE / 'data' / f'{name}.folded'
    if not folded.exists():
        script = subprocess.Popen(['perf', 'script', '--inline', '-i', str(HERE / 'data' / f'{name}.data')],
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        with folded.open('w') as out:
            subprocess.run(['perl', str(FG / 'stackcollapse-perf.pl')], stdin=script.stdout,
                           stdout=out, stderr=subprocess.DEVNULL, check=True)
    total = 0
    gc = collections.Counter()
    for line in folded.read_text().splitlines():
        stack, count = line.rsplit(' ', 1)
        count = int(count)
        total += count
        frames = stack.split(';')
        if 'gc_collect_main' not in frames:
            continue
        frames = frames[frames.index('gc_collect_main'):]
        frames = [f for f in frames if not TRIVIAL.match(f)]
        if (len(frames) > 2 and frames[1] == 'untrack_tuples'
                and (frames[2] == 'visit_reachable' or frames[2].endswith('_traverse'))):
            frames = ['gc_collect_main', 'deduce_unreachable', 'move_unreachable'] + frames[2:]
        if 'move_unreachable' in frames:
            frames = ['gc_collect_main', 'deduce_unreachable'] + frames[frames.index('move_unreachable'):]
        gc[';'.join(frames)] += count
    return total, gc


for work in WORK:
    results = {who: fold(f'{work}-{who}') for who in LABELS}
    scale = sum(results['base'][1].values())
    for who, (total, gc) in results.items():
        gc_total = sum(gc.values())
        text = HERE / 'data' / f'{work}-{who}.gc.folded'
        text.write_text(''.join(f'{k} {v}\n' for k, v in sorted(gc.items())))
        title = f'Garbage collector only: {WORK[work]} ({LABELS[who]})'
        sub = (f'{gc_total / 1e9:.2f}G cycles in the collector, {100 * gc_total / total:.0f}% of the process'
               + ('' if who == 'base' else f'; {100 * (1 - gc_total / scale):.0f}% less than main, same scale as main'))
        svg = HERE / f'{work}-{who}.svg'
        with svg.open('w') as out:
            subprocess.run(['perl', str(FG / 'flamegraph.pl'), '--title', title, '--subtitle', sub,
                            '--width', '1400', '--countname', 'cycles', '--total', str(scale),
                            '--cp', '--minwidth', '0.2', str(text)],
                           stdout=out, cwd=HERE / 'data', check=True)
        subprocess.run(['rsvg-convert', '-o', str(svg.with_suffix('.png')), str(svg)], check=True)
        print(f'{work:7} {LABELS[who]:10} collector {gc_total / 1e9:6.2f}G cycles ({100 * gc_total / total:4.1f}% of process)')
