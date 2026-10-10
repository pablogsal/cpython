from sympy import symbols, expand, factor, diff, integrate, sin, cos, exp, Matrix, simplify

x, y, z = symbols('x y z')
p = expand((x + y + z + 1) ** 18)
q = diff(p, x, 2)
r = factor(expand((x + y) ** 9 * (x - z) ** 6))
m = Matrix(6, 6, lambda i, j: x ** i + y * j + int(i == j))
d = m.det()
s = simplify(sum(sin(x) ** 2 * cos(x) ** k for k in range(8)))
i = integrate(exp(-x) * x ** 6 * sin(x), x)
print(len(p.args), len(q.args), len(d.args), r.is_Mul)
