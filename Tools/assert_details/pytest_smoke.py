"""Exercise the optional pytest adapter in isolated subprocesses."""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


HELPER = '''
class Box:
    def __init__(self, value): self.value = value
    def __eq__(self, other): return self.value == other.value

def check(left, right):
    "helper docstring"
    assert left == right
'''

CONFTEST = '''
import pytest
pytest.register_assert_rewrite('native_helper')
from native_helper import Box

def pytest_assertrepr_compare(config, op, left, right):
    if isinstance(left, Box) and isinstance(right, Box):
        return [f'custom boxes: {left.value} {op} {right.value}']
'''

TESTS = '''
import dis
import sys
import pytest
import assertionlib
from native_helper import Box, check

def test_caught_message():
    with pytest.raises(AssertionError, match='custom boxes: 1 == 2') as caught:
        assert Box(1) == Box(2)
    assert caught.value.details is not None

def test_registered_helper():
    with pytest.raises(AssertionError, match='custom boxes: 3 == 4'):
        check(Box(3), Box(4))

def test_compiler_backend():
    names = {i.opname for i in dis.get_instructions(check)}
    assert 'CALL_INTRINSIC_2' in names
    assert check.__doc__ == 'helper docstring'
    assert not any(name.startswith('@pytest') for name in check.__globals__)

def test_pass_and_debug():
    assert 2 == 2
    assert __debug__

def test_evaluation_once():
    events = []
    class Flag:
        def __bool__(self):
            events.append('bool')
            return False
    with pytest.raises(AssertionError):
        assert Flag() and events.append('skipped')
    assert events == ['bool']

def test_report():
    assert Box(10) == Box(20)
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pytest-source', type=Path, required=True)
    parser.add_argument('--pytest-deps', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env['PYTEST_DISABLE_PLUGIN_AUTOLOAD'] = '1'
    env['PYTHONPATH'] = os.pathsep.join(map(str, (
        Path(__file__).resolve().parent, args.pytest_source / 'src', args.pytest_deps,
    )))
    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        for name, source in [('native_helper.py', HELPER), ('conftest.py', CONFTEST),
                             ('test_native.py', TESTS)]:
            (directory / name).write_text(source)
        for index, optimize in enumerate(('', '-O', '-OO', '')):
            command = [sys.executable]
            if optimize:
                command.append(optimize)
            command += ['-m', 'pytest', '-p', 'pytest_native', '-q']
            result = subprocess.run(command, cwd=directory, env=env,
                                    text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            (args.output / f'pytest-{index}.log').write_text(result.stdout)
            if (result.returncode != 1 or '1 failed, 5 passed' not in result.stdout or
                    'custom boxes: 10 == 20' not in result.stdout or
                    '<native_helper.Box object' in result.stdout):
                raise RuntimeError(f'pytest integration failed: {command}\n{result.stdout}')
            print(f'{optimize or "normal"}: expected report and hooks passed', flush=True)


if __name__ == '__main__':
    main()
