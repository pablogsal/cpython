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
# print("Extended assert code:")
# print(ast.unparse(tree.body[0].extended_assert))
# print()

# Now compile and execute the extended_assert
print("Compiling and executing the extended_assert...")
# Create a new module with just the extended statements
extended_module = ast.Module(body=tree.body[0].extended_assert, type_ignores=[])
ast.fix_missing_locations(extended_module)

# Compile and execute
code = compile(extended_module, '<assert>', 'exec')
print(ast.unparse(extended_module))
exec(code, globals())

print("✅ Assertion passed! Extended assert executed successfully.")
