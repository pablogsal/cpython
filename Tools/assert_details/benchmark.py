"""Measure native assertion captures against plain and pytest assertions.

Run with the prototype interpreter. Timed code uses compile() flags directly,
so all modes share the same executable, library, CPU, and optimization level.
"""

import argparse
import ast
import dis
import gc
import io
import json
import marshal
import os
from pathlib import Path
import random
import statistics
import sys
import sysconfig
import time
import timeit
import traceback
import types


CASES = {
    'constant': ('True', ()),
    'equal_int': ('a == b', (123, 123)),
    'equal_large_int': ('a == b', (10**50, int(str(10**50)))),
    'arithmetic_large_int': ('a * 2 + b == expected', (10**50, 10**49, 21 * 10**49)),
    'arithmetic_float': ('a * 2 + b == expected', (2.25, 5.5, 10.0)),
    'arithmetic': ('a * 2 + b == expected', (11, 20, 42)),
    'attribute': ('a.value == b', (types.SimpleNamespace(value=42), 42)),
    'subscript': ('a["value"] == b', ({'value': 42}, 42)),
    'call': ('normalize(a) == b', (' ALICE ', 'alice')),
    'method': ('a.strip().lower() == b', (' ALICE ', 'alice')),
    'boolean_and': ('a > 0 and b > 0 and expected > 0', (1, 2, 3)),
    'boolean_or_short': ('a > 0 or b > 0', (1, 2)),
    'comparison_chain': ('0 < a < b < expected', (1, 2, 3)),
    'list_equal': ('a == b', (list(range(32)), list(range(32)))),
    'generator_all': ('all(x >= 0 for x in a)', (list(range(12)),)),
}


def build(expression, mode, rewrite=None, iterations=1):
    source = ('def normalize(value):\n    return value.strip().lower()\n\n'
              'def check(a=None, b=None, expected=None):\n'
              + (f'    assert {expression}\n' if iterations == 1 else
                 f'    for _ in range({iterations}):\n        assert {expression}\n'))
    tree = ast.parse(source)
    if mode == 'pytest':
        rewrite(tree, source.encode())
    flags = ast.PyCF_ASSERT_DETAILS if mode == 'native' else 0
    code = compile(tree, '<benchmark>', 'exec', flags=flags, dont_inherit=True)
    ns = {}
    exec(code, ns)
    return ns['check']


def sizes(code):
    codes = [code]
    for constant in code.co_consts:
        if isinstance(constant, types.CodeType):
            codes.extend(walk_codes(constant))
    return {
        'bytecode_bytes': sum(len(c.co_code) for c in codes),
        'instructions': sum(len(list(dis.get_instructions(c))) for c in codes),
        'cache_units': sum(sum(i.opname == 'CACHE'
                              for i in dis.get_instructions(c, show_caches=True))
                           for c in codes),
        'exception_table_bytes': sum(len(c.co_exceptiontable) for c in codes),
        'location_table_bytes': sum(len(c.co_linetable) for c in codes),
        'marshal_bytes': len(marshal.dumps(code)),
        'frame_slots': code.co_nlocals + code.co_stacksize,
        'locals': code.co_nlocals,
        'stacksize': code.co_stacksize,
    }


def walk_codes(code):
    yield code
    for constant in code.co_consts:
        if isinstance(constant, types.CodeType):
            yield from walk_codes(constant)


def calibrate(timer, minimum):
    number = 1000
    while True:
        elapsed = timer.timeit(number)
        if elapsed >= minimum / 3:
            return max(1000, int(number * minimum / elapsed))
        number *= 5


def pin_idle_cpu():
    if not hasattr(os, 'sched_getaffinity'):
        return None
    allowed = os.sched_getaffinity(0)
    def counters():
        result = {}
        for line in Path('/proc/stat').read_text().splitlines():
            name, *fields = line.split()
            if name.startswith('cpu') and name[3:].isdigit():
                cpu = int(name[3:])
                values = list(map(int, fields))
                result[cpu] = (sum(values[:8]), values[3] + values[4])
        return result
    first = counters()
    time.sleep(0.5)
    second = counters()
    busy = {cpu: 1 - (second[cpu][1] - first[cpu][1]) /
            max(1, second[cpu][0] - first[cpu][0]) for cpu in allowed}
    cpu = min(busy, key=busy.get)
    os.sched_setaffinity(0, {cpu})
    return cpu


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--pytest-source', type=Path)
    parser.add_argument('--pytest-deps', type=Path)
    parser.add_argument('--iterations', type=int, default=1)
    parser.add_argument('--cpu', type=int)
    parser.add_argument('--callbacks', action='store_true')
    parser.add_argument('--cases', nargs='+', choices=CASES)
    parser.add_argument('--rounds', type=int, default=9)
    parser.add_argument('--sample-seconds', type=float, default=0.15)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    modes = ['plain', 'native']
    rewrite = None
    if args.pytest_source:
        sys.path.insert(0, str(args.pytest_source / 'src'))
        if args.pytest_deps:
            sys.path.append(str(args.pytest_deps))
        from _pytest.assertion.rewrite import rewrite_asserts
        rewrite = rewrite_asserts
        modes.append('pytest')
    if args.cpu is None:
        cpu = pin_idle_cpu()
    else:
        cpu = args.cpu
        os.sched_setaffinity(0, {cpu})
    results = {
        'python': sys.version,
        'executable': sys.executable,
        'config_args': sysconfig.get_config_var('CONFIG_ARGS'),
        'cflags': sysconfig.get_config_var('CFLAGS'),
        'cpu': cpu,
        'loadavg': os.getloadavg(),
        'rounds': args.rounds,
        'iterations_per_call': args.iterations,
        'sample_seconds': args.sample_seconds,
        'cases': {},
    }
    disassembly = io.StringIO()
    rng = random.Random(20261018)
    for name, (expression, call_args) in CASES.items():
        if args.cases and name not in args.cases:
            continue
        case = results['cases'][name] = {'expression': expression, 'modes': {}}
        timers = {}
        for mode in modes:
            fn = build(expression, mode, rewrite, args.iterations)
            for _ in range(max(5, 5000 // args.iterations)):
                fn(*call_args)
            timer = timeit.Timer('fn(*call_args)', globals={'fn': fn, 'call_args': call_args})
            count = calibrate(timer, args.sample_seconds)
            timers[mode] = (timer, count)
            case['modes'][mode] = {'sizes': sizes(fn.__code__), 'samples_ns': [], 'loops': count}
            print(f'\n{name} / {mode}\n', file=disassembly)
            dis.dis(fn, file=disassembly, show_caches=True)
        for _ in range(args.rounds):
            order = modes.copy()
            rng.shuffle(order)
            for mode in order:
                timer, count = timers[mode]
                elapsed = timer.timeit(count)
                case['modes'][mode]['samples_ns'].append(elapsed * 1e9 / count / args.iterations)
        for mode, data in case['modes'].items():
            data['median_ns'] = statistics.median(data['samples_ns'])
            data['mad_ns'] = statistics.median(abs(s - data['median_ns']) for s in data['samples_ns'])
        plain = case['modes']['plain']['median_ns']
        for data in case['modes'].values():
            data['relative_time'] = data['median_ns'] / plain
        print(name, {mode: round(data['relative_time'], 3)
                     for mode, data in case['modes'].items()}, flush=True)
        (args.output / 'benchmark.json').write_text(json.dumps(results, indent=2))
    (args.output / 'disassembly.txt').write_text(disassembly.getvalue())
    # Separate exception creation/capture from traceback formatting.
    failure = results['failure'] = {}
    for mode in modes:
        fn = build('a == b', mode, rewrite)
        def catch():
            try:
                fn(1, 2)
            except AssertionError:
                pass
        def display():
            try:
                fn(1, 2)
            except AssertionError as exc:
                traceback.format_exception(exc)
        failure[mode] = {}
        for label, action in [('raise_catch', catch), ('raise_format', display)]:
            module_loaded = 'assertionlib' in sys.modules
            timer = timeit.Timer(action)
            loops = calibrate(timer, args.sample_seconds)
            samples = [timer.timeit(loops) * 1e9 / loops for _ in range(args.rounds)]
            failure[mode][label] = {
                'assertionlib_loaded_before': module_loaded,
                'samples_ns': samples, 'median_ns': statistics.median(samples),
            }
    if args.callbacks:
        import assertionlib
        callback_results = results['callbacks'] = {}
        failed_fn = build('a == b', 'native')
        def failed():
            try:
                failed_fn(1, 2)
            except AssertionError:
                pass
        cases = [
            ('failure_no_handler', failed, {}),
            ('failure_handler', failed, {'on_failure': lambda error, details: None}),
            ('failure_format_handler', failed,
             {'on_failure': lambda error, details: assertionlib.format_details(details)}),
        ]
        for name, action, handlers in cases:
            with assertionlib.handlers(**handlers):
                timer = timeit.Timer(action)
                count = calibrate(timer, args.sample_seconds)
                samples = [timer.timeit(count) * 1e9 / count for _ in range(args.rounds)]
            callback_results[name] = {
                'samples_ns': samples, 'median_ns': statistics.median(samples),
            }
    (args.output / 'benchmark.json').write_text(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
