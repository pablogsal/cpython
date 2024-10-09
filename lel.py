import ast


def compare(left, op, right):
    ops = {
        '==': lambda a, b: a == b,
        '!=': lambda a, b: a != b,
        '<': lambda a, b: a < b,
        '<=': lambda a, b: a <= b,
        '>': lambda a, b: a > b,
        '>=': lambda a, b: a >= b,
        'is': lambda a, b: a is b,
        'is not': lambda a, b: a is not b,
        'in': lambda a, b: a in b,
        'not in': lambda a, b: a not in b
    }
    return ops[op](left, right)


x = 4
y = 3
z = 2
w = 1

tree = ast.parse("assert x > y > z > w")
print(ast.unparse(tree.body[0].extended_assert))
