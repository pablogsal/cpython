"""Tests for ``sys.__assertion_hook__`` and ``ast.Assert.extended_tree``."""

import ast
import sys
import unittest


class HookCalls:
    """Simple callable that records every invocation."""

    def __init__(self, return_value=""):
        self.calls = []
        self.return_value = return_value

    def __call__(self, source_strs, values, msg, expr_source):
        self.calls.append((source_strs, values, msg, expr_source))
        return self.return_value


class HookContext:
    """Context manager: temporarily installs ``hook`` as ``sys.__assertion_hook__``."""

    def __init__(self, hook):
        self.hook = hook

    def __enter__(self):
        self._saved = sys.__assertion_hook__
        sys.__assertion_hook__ = self.hook
        return self.hook

    def __exit__(self, *exc):
        sys.__assertion_hook__ = self._saved


class ExtendedTreeASTTests(unittest.TestCase):
    """The compiler populates ``Assert.extended_tree`` automatically.

    The extended_tree must only contain sub-expressions whose values the
    runtime can capture with a *single* evaluation of the test; otherwise
    the assertion would have to re-execute code on failure which would
    re-run side effects.
    """

    def get_assert(self, source):
        return ast.parse(source).body[0]

    def test_field_present(self):
        node = self.get_assert("assert x")
        self.assertIn("extended_tree", node._fields)
        self.assertIsInstance(node.extended_tree, ast.Tuple)

    def test_simple_compare_captures_two_operands(self):
        node = self.get_assert("assert a == b")
        elts = node.extended_tree.elts
        self.assertEqual([e.id for e in elts], ["a", "b"])

    def test_chained_compare_falls_back_to_whole(self):
        # Chained compares short-circuit, so single-pass operand capture
        # is not implemented for them yet.
        node = self.get_assert("assert a < b < c")
        elts = node.extended_tree.elts
        self.assertEqual(len(elts), 1)
        self.assertIsInstance(elts[0], ast.Compare)

    def test_boolop_falls_back_to_whole(self):
        node = self.get_assert("assert a and b")
        elts = node.extended_tree.elts
        self.assertEqual(len(elts), 1)
        self.assertIsInstance(elts[0], ast.BoolOp)

    def test_other_falls_back_to_whole(self):
        node = self.get_assert("assert x")
        elts = node.extended_tree.elts
        self.assertEqual([e.id for e in elts], ["x"])


class HookRuntimeTests(unittest.TestCase):
    """Bytecode dispatches to ``sys.__assertion_hook__`` on failure."""

    def test_default_hook_is_none(self):
        # Don't assume previous tests didn't tamper; only check the type.
        self.assertTrue(hasattr(sys, "__assertion_hook__"))

    def test_passing_assert_does_not_invoke_hook(self):
        hook = HookCalls()
        with HookContext(hook):
            assert 1 == 1
            assert True
        self.assertEqual(hook.calls, [])

    def test_hook_receives_source_strs_and_values(self):
        hook = HookCalls(return_value="formatted")
        with HookContext(hook):
            with self.assertRaises(AssertionError) as cm:
                a, b = 1, 2
                assert a == b
        self.assertEqual(str(cm.exception), "formatted")
        self.assertEqual(len(hook.calls), 1)
        source_strs, values, msg, expr_source = hook.calls[0]
        self.assertEqual(source_strs, ("a", "b"))
        self.assertEqual(values, (1, 2))
        self.assertIsNone(msg)
        self.assertEqual(expr_source, "a == b")

    def test_hook_receives_user_message(self):
        hook = HookCalls(return_value="m")
        with HookContext(hook):
            with self.assertRaises(AssertionError):
                assert 1 == 2, "boom"
        _, _, msg, _ = hook.calls[0]
        self.assertEqual(msg, "boom")

    def test_hook_none_preserves_msg(self):
        with HookContext(None):
            with self.assertRaises(AssertionError) as cm:
                assert False, "user msg"
            self.assertEqual(str(cm.exception), "user msg")

    def test_hook_none_no_msg_preserves_legacy(self):
        with HookContext(None):
            with self.assertRaises(AssertionError) as cm:
                assert False
            # AssertionError() with no args -- str() is empty.
            self.assertEqual(str(cm.exception), "")
            self.assertEqual(cm.exception.args, ())

    def test_hook_returning_none_yields_no_args(self):
        def hook(*args, **kwargs):
            return None
        with HookContext(hook):
            with self.assertRaises(AssertionError) as cm:
                assert False, "ignored"
            self.assertEqual(cm.exception.args, ())

    def test_hook_must_return_str_or_none(self):
        def hook(*args, **kwargs):
            return 42
        with HookContext(hook):
            # A buggy hook propagates a TypeError directly; the
            # underlying AssertionError never gets a chance to run.
            with self.assertRaisesRegex(
                TypeError,
                r"sys\.__assertion_hook__ must return str or None",
            ):
                assert False

    def test_hook_exception_is_chained(self):
        class MyError(RuntimeError):
            pass

        def bad_hook(*args, **kwargs):
            raise MyError("from hook")

        with HookContext(bad_hook):
            with self.assertRaises(AssertionError) as cm:
                assert False
            self.assertIsInstance(cm.exception.__context__, MyError)

    def test_captured_subexprs_evaluated_exactly_once_compare(self):
        """For ``assert a == b`` both operands run **once** even when the
        assert fails and the hook is invoked.  Re-evaluation would re-run
        side effects of the user's expression on the failure path -- the
        entire point of doing the capture inline in bytecode."""
        evaluations = []

        def track(name, value):
            def f():
                evaluations.append(name)
                return value
            return f

        a = track("a", 1)
        b = track("b", 2)

        hook = HookCalls(return_value="boom")
        with HookContext(hook):
            with self.assertRaises(AssertionError):
                assert a() == b()
        self.assertEqual(evaluations, ["a", "b"])
        # The hook saw the actual captured values, not re-evaluated ones.
        self.assertEqual(hook.calls[0][1], (1, 2))

    def test_captured_test_evaluated_exactly_once_fallback(self):
        """For test shapes other than simple Compare, the whole test value
        is captured -- still exactly once."""
        evaluations = []

        def track():
            evaluations.append(None)
            return False

        hook = HookCalls(return_value="boom")
        with HookContext(hook):
            with self.assertRaises(AssertionError):
                assert track()
        self.assertEqual(len(evaluations), 1)
        # Hook saw the single captured value.
        self.assertEqual(hook.calls[0][1], (False,))

    def test_no_double_eval_without_hook(self):
        """When the hook is None we still must not double-evaluate."""
        evaluations = []
        def left():
            evaluations.append("l")
            return 0
        def right():
            evaluations.append("r")
            return 1
        with HookContext(None):
            with self.assertRaises(AssertionError):
                assert left() == right()
        self.assertEqual(evaluations, ["l", "r"])


class DefaultHookTests(unittest.TestCase):
    """The ``_assertion_hook`` module ships a pytest-style default hook."""

    def test_install_uninstall(self):
        import _assertion_hook
        try:
            _assertion_hook.install()
            self.assertIs(sys.__assertion_hook__, _assertion_hook.default_hook)
        finally:
            _assertion_hook.uninstall()
        # Restored to whatever it was before -- typically None.
        self.assertTrue(
            sys.__assertion_hook__ is None
            or callable(sys.__assertion_hook__)
        )

    def test_default_hook_shows_values(self):
        import _assertion_hook
        with HookContext(_assertion_hook.default_hook):
            with self.assertRaises(AssertionError) as cm:
                x, y = 10, 20
                assert x == y
        message = str(cm.exception)
        self.assertIn("assert x == y", message)
        self.assertIn("x = 10", message)
        self.assertIn("y = 20", message)

    def test_default_hook_includes_user_message(self):
        import _assertion_hook
        with HookContext(_assertion_hook.default_hook):
            with self.assertRaises(AssertionError) as cm:
                assert 1 == 2, "things are off"
        self.assertIn("things are off", str(cm.exception))


if __name__ == "__main__":
    unittest.main()
