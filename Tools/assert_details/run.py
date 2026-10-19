"""Run one source file with native assertion diagnostics enabled."""
import argparse
import ast
from pathlib import Path
import sys
import types


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('file', type=Path)
    parser.add_argument('args', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    path = args.file.resolve()
    code = compile(path.read_bytes(), str(path), 'exec',
                   flags=ast.PyCF_ASSERT_DETAILS, dont_inherit=True)
    sys.argv = [str(path), *args.args]
    sys.path[0] = str(path.parent)
    module = types.ModuleType('__main__')
    module.__file__ = str(path)
    module.__package__ = None
    module.__spec__ = None
    sys.modules['__main__'] = module
    exec(code, module.__dict__)


if __name__ == '__main__':
    main()
