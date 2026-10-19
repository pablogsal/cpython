"""Compare code size and compile time on assertion-bearing library modules."""
import argparse
import ast
import json
import marshal
from pathlib import Path
import statistics
import subprocess
import time
import types


def code_totals(code):
    result = dict(code_objects=1, bytecode_bytes=len(code.co_code),
                  exception_table_bytes=len(code.co_exceptiontable),
                  location_table_bytes=len(code.co_linetable),
                  locals=code.co_nlocals, stacksize=code.co_stacksize)
    for value in code.co_consts:
        if isinstance(value, types.CodeType):
            for key, count in code_totals(value).items():
                result[key] += count
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    paths = subprocess.check_output(['git', 'ls-files', 'Lib'], text=True).splitlines()
    sources = []
    for name in paths:
        path = Path(name)
        if path.suffix != '.py' or any(p.startswith('test') or p.endswith('_test') for p in path.parts[1:-1]):
            continue
        source = path.read_bytes()
        tree = ast.parse(source, name)
        count = sum(isinstance(node, ast.Assert) for node in ast.walk(tree))
        if count:
            sources.append((name, source, count))
    rows = []
    totals = {mode: {} for mode in ('plain', 'native')}
    for name, source, count in sources:
        row = {'file': name, 'assertions': count}
        for mode, flags in [('plain', 0), ('native', ast.PyCF_ASSERT_DETAILS)]:
            code = compile(source, name, 'exec', flags=flags, dont_inherit=True)
            sizes = code_totals(code)
            sizes['marshal_bytes'] = len(marshal.dumps(code))
            row[mode] = sizes
            for key, value in sizes.items():
                totals[mode][key] = totals[mode].get(key, 0) + value
        rows.append(row)
    times = {'plain': [], 'native': []}
    for trial in range(8):
        order = [('plain', 0), ('native', ast.PyCF_ASSERT_DETAILS)]
        if trial % 2:
            order.reverse()
        for mode, flags in order:
            start = time.perf_counter()
            for name, source, _ in sources:
                compile(source, name, 'exec', flags=flags, dont_inherit=True)
            times[mode].append(time.perf_counter() - start)
    result = dict(files=len(rows), assertions=sum(r['assertions'] for r in rows),
                  totals=totals, compile_seconds=times,
                  compile_median={k: statistics.median(v) for k,v in times.items()}, rows=rows)
    args.output.write_text(json.dumps(result, indent=2))
    print(json.dumps({k: v for k,v in result.items() if k != 'rows'}, indent=2))


if __name__ == '__main__':
    main()
