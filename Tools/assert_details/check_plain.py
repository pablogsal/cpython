"""Emit stable code signatures for plain assertions on two interpreter builds."""
import ast
import dis
import hashlib
import json
from pathlib import Path
import subprocess
import types


def signature(code):
    return {
        'bytecode': [(i.opname, i.arg) for i in dis.get_instructions(code, show_caches=True)],
        'exceptions': code.co_exceptiontable.hex(),
        'locations': code.co_linetable.hex(),
        'locals': code.co_varnames,
        'names': code.co_names,
        'cells': code.co_cellvars,
        'free': code.co_freevars,
        'flags': code.co_flags,
        'stack': code.co_stacksize,
        'consts': [signature(c) if isinstance(c, types.CodeType)
                   else (type(c).__name__, repr(c)) for c in code.co_consts],
    }


paths = subprocess.check_output(['git', 'ls-files', 'Lib'], text=True).splitlines()
results = {}
for name in paths:
    path = Path(name)
    if path.suffix != '.py' or any(p.startswith('test') or p.endswith('_test') for p in path.parts[1:-1]):
        continue
    source = path.read_bytes()
    if not any(isinstance(n, ast.Assert) for n in ast.walk(ast.parse(source, name))):
        continue
    code = compile(source, name, 'exec', dont_inherit=True)
    data = json.dumps(signature(code), sort_keys=True).encode()
    results[name] = hashlib.sha256(data).hexdigest()
print(json.dumps(results, indent=2, sort_keys=True))
