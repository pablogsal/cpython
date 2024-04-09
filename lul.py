import rewrite as sys, builtins as py_builtins

py_assert1 = 1
py_assert3 = x + py_assert1
py_assert5 = py_assert3 + z
py_assert6 = py_assert5 == y
if not py_assert6:
    py_format8 = sys._call_reprcompare(
        ("==",),
        (py_assert6,),
        ("((%(py0)s + %(py2)s) + %(py4)s) == %(py7)s",),
        (py_assert5, y),
    ) % {
        "py0": sys._saferepr(x)
        if "x" in py_builtins.locals() or sys._should_repr_global_name(x)
        else "x",
        "py2": sys._saferepr(py_assert1),
        "py4": sys._saferepr(z)
        if "z" in py_builtins.locals() or sys._should_repr_global_name(z)
        else "z",
        "py7": sys._saferepr(y)
        if "y" in py_builtins.locals() or sys._should_repr_global_name(y)
        else "y",
    }
    py_format10 = ("" + "assert %(py9)s") % {"py9": py_format8}
    raise py_builtins.AssertionError(sys._format_explanation(py_format10))
py_assert1 = py_assert3 = py_assert5 = py_assert6 = None
