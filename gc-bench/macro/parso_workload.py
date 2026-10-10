import os
import pathlib
import parso

LIB = pathlib.Path(os.__file__).parent
trees = [parso.parse(p.read_text(encoding='utf-8')) for p in sorted(LIB.glob('*.py'))]
print(len(trees), sum(1 for t in trees for _ in t.iter_funcdefs()))
