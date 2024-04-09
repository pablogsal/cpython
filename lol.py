import sys,ast
import rewrite

def foo(x):
    r = rewrite.AssertionRewriter()
    r.visit(x)
    x2 = ast.Module(r.statements)
    print("Modified ast code: ")
    print(ast.unparse(x2))
    print()
    return x2

# sys.setassertrewritter(foo)

import lel

lel.bar(1,1)
