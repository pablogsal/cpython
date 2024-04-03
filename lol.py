import sys,ast
import rewrite

def foo(x):
    r = rewrite.AssertionRewriter()
    r.visit(x)
    print(ast.dump(x))
    return x
sys.setassertrewritter(foo)

import lel

lel.bar(1,1)
