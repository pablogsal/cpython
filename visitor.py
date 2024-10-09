import ast


class ComparisonTransformer(ast.NodeTransformer):
    def visit_Compare(self, node):
        self.generic_visit(node)

        if len(node.ops) == 1 and len(node.comparators) == 1:
            return self.transform_single_comparison(node)
        else:
            return self.transform_chained_comparison(node)

    def transform_single_comparison(self, node):
        return ast.Call(
            func=ast.Name(id='compare', ctx=ast.Load()),
            args=[
                node.left,
                ast.Constant(value=self.get_op_symbol(node.ops[0])),
                node.comparators[0]
            ],
            keywords=[]
        )

    def transform_chained_comparison(self, node):
        result = None
        for i, (op, right) in enumerate(zip(node.ops, node.comparators)):
            left = node.left if i == 0 else ast.Name(
                id=f'_tmp{i-1}', ctx=ast.Load())
            comparison = ast.Call(
                func=ast.Name(id='compare', ctx=ast.Load()),
                args=[
                    left,
                    ast.Constant(value=self.get_op_symbol(op)),
                    right
                ],
                keywords=[]
            )
            if result is None:
                result = ast.NamedExpr(
                    target=ast.Name(id=f'_tmp{i}', ctx=ast.Store()),
                    value=comparison
                )
            else:
                result = ast.BoolOp(
                    op=ast.And(),
                    values=[
                        result,
                        ast.NamedExpr(
                            target=ast.Name(id=f'_tmp{i}', ctx=ast.Store()),
                            value=comparison
                        )
                    ]
                )
        return result

    def visit_Assert(self, node):
        inner = self.visit(node.test)
        the_lambda = ast.Lambda(
            args=ast.arguments(
                posonlyargs=[], args=[], kwonlyargs=[], kw_defaults=[], defaults=[]
            ),
            body=inner
        )
        # Generate a call to the lambda
        return ast.Expr(
            value=ast.Call(
                func=the_lambda,
                args=[],
                keywords=[]
            )
        )

    def get_op_symbol(self, op):
        op_map = {
            ast.Eq: '==', ast.NotEq: '!=', ast.Lt: '<', ast.LtE: '<=',
            ast.Gt: '>', ast.GtE: '>=', ast.Is: 'is', ast.IsNot: 'is not',
            ast.In: 'in', ast.NotIn: 'not in'
        }
        return op_map[type(op)]


def transform_code(code):
    tree = ast.parse(code)
    transformer = ComparisonTransformer()
    new_tree = transformer.visit(tree)
    ast.fix_missing_locations(new_tree)
    print(ast.unparse(new_tree))
    return new_tree


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


def execute_ast(tree, globals_dict=None, locals_dict=None):
    if globals_dict is None:
        globals_dict = {}
    if locals_dict is None:
        locals_dict = {}

    globals_dict['compare'] = compare

    # Wrap the tree in a module if it's not already
    if not isinstance(tree, ast.Module):
        tree = ast.Module(body=[tree], type_ignores=[])

    code = compile(tree, '<ast>', 'exec')
    exec(code, globals_dict, locals_dict)
    return locals_dict


test_cases = [
    ("assert 0 < x < y <= 10", {'x': 5, 'y': 7}),
    ("assert 0 < x < y <= 10", {'x': 5, 'y': 15}),
    ("assert 0 < x < y <= 10", {'x': 15, 'y': 20}),
    ("assert x > 0 and y < 10 and x < y", {'x': 5, 'y': 7}),
    ("assert x > 0 and y < 10 and x < y", {'x': 7, 'y': 5}),
]

for code, variables in test_cases:
    print(f"Testing: {code}")
    print(f"Variables: {variables}")
    try:
        tree = transform_code(code)
        result = execute_ast(tree, globals_dict=variables.copy())
        print("Assertion passed")
    except AssertionError:
        print("Assertion failed")
    print()
