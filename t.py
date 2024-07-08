import ast


def compare(x):
    a = ast.dump(ast.parse(x))
    b = ast.dump(ast.parse(ast.unparse(ast.parse(x))))
    print(ast.unparse(ast.parse(x)))
    return a == b


print(compare('f"{datetime.datetime.now():h1{y=}h2{y=}h3{y=}}"'))
