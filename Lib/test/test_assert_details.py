"""Semantic checks for the native assertion diagnostic prototype."""

import ast
import asyncio
import gc
import marshal
import pickle
import pprint
import sys
import traceback
import unittest
import weakref
from test.support import requires_jit_enabled

import assertionlib


FLAGS = ast.PyCF_ASSERT_DETAILS


def execute(source, *, flags=FLAGS, optimize=0, namespace=None):
    if namespace is None:
        namespace = {}
    code = compile(source, '<assert-details>', 'exec', flags=flags,
                   dont_inherit=True, optimize=optimize)
    exec(code, namespace)
    return namespace


def fail(source, namespace=None):
    try:
        execute(source, namespace=namespace)
    except AssertionError as exc:
        return exc
    raise RuntimeError('assertion did not fail')


class AssertionDetailsTests(unittest.TestCase):
    def test_comparison(self):
        exc = fail('left = 4\nright = 7\nassert left == right')
        self.assertEqual(exc.args, ())
        self.assertEqual(str(exc), '')
        text = exc.details.format()
        self.assertEqual(text, 'assert 4 == 7')

    def test_full_expression(self):
        ns = execute('''
class User:
    name = 'ALICE'
def normalize(name):
    return name.lower()
user = User()
expected = 'bob'
''')
        exc = fail('assert normalize(user.name) == expected and user.missing', ns)
        text = exc.details.format()
        self.assertIn("assert ('alice' == 'bob')", text)
        self.assertIn("+ where 'alice' = normalize('ALICE')", text)
        self.assertIn("+ where 'ALICE' = ", text)
        self.assertNotIn('missing', text)

    def test_control_flow_matches_plain(self):
        setup = '''
events = []
class Flag:
    def __init__(self, value): self.value = value
    def __bool__(self):
        events.append(('bool', self.value))
        return self.value
    def __repr__(self):
        events.append('repr')
        return 'Flag()'
def call(name, value):
    events.append(name)
    return value
f = Flag(False)
t = Flag(True)
'''
        expressions = [
            'f and call("skip", True)',
            't or call("skip", False)',
            '(f or t) and f',
            'not (t or f)',
            '(f and t) or f',
            'call("a", 3) < call("b", 2) < call("c", 1)',
            'call("a", 1) < call("b", 2) < call("c", 1)',
            '(f if call("test", True) else t)',
            '(t if call("test", False) else f)',
            '(f and t) == f',
            'not (f and t)',
        ]
        for expression in expressions:
            with self.subTest(expression=expression):
                results = []
                for flags in (0, FLAGS):
                    ns = {}
                    try:
                        execute(setup + '\nassert ' + expression, flags=flags,
                                namespace=ns)
                    except BaseException as exc:
                        outcome = type(exc)
                    else:
                        outcome = None
                    results.append((outcome, ns['events']))
                self.assertEqual(*results)

    def test_custom_comparison_truth(self):
        ns = execute('''
events = []
class Result:
    def __bool__(self):
        events.append('bool')
        return False
    def __repr__(self): return 'Result()'
class Value:
    def __lt__(self, other):
        events.append('lt')
        return Result()
a = Value()
b = Value()
''')
        exc = fail('assert a < b < 0', ns)
        self.assertEqual(ns['events'], ['lt', 'bool'])
        exc.details.format()
        self.assertEqual(ns['events'], ['lt', 'bool'])

    def test_message_identity_and_order(self):
        marker = object()
        ns = {'marker': marker, 'events': []}
        exc = fail('assert events.append("condition"), '
                   '(events.append("message"), marker)[1]', ns)
        self.assertIs(exc.args[0], marker)
        self.assertEqual(ns['events'], ['condition', 'message'])
        execute('assert True, 1/0')
        with self.assertRaises(ZeroDivisionError):
            execute('assert False, 1/0')

    def test_expression_exception(self):
        with self.assertRaisesRegex(ValueError, 'source'):
            execute('def f(): raise ValueError("source")\nassert f()')

    def test_named_expression_capture(self):
        exc = fail('x = 2\nassert x + (x := 3) == 6')
        values = {node.text: node.value for node in exc.details.nodes
                  if node.evaluated}
        self.assertEqual(values['x'], 2)
        self.assertEqual(values['(x := 3)'], 3)
        self.assertEqual(values['x + (x := 3)'], 5)

    def test_calls_and_subscripts(self):
        exc = fail('''
def f(*args, **kwargs): return args[0] + kwargs['n']
data = [10, 20]
assert f(*data[:1], **{'n': 2}) * 2 == data[1]
''')
        text = exc.details.format()
        self.assertIn('assert (12 * 2) == 20', text)
        self.assertIn("+ where 12 = f(*[10], **{'n': 2})", text)
        self.assertIn('+ and 20 = [10, 20][1]', text)

    def test_scopes_do_not_expose_captures(self):
        ns = execute('''
assert all(not key.startswith('.assertion.') for key in locals())
class C:
    assert all(not key.startswith('.assertion.') for key in locals())
def f():
    assert 1 == 1 and all(not k.startswith('.assertion.') for k in locals())
    return locals()
result = f()
''')
        self.assertFalse(any(k.startswith('.assertion.') for k in ns))
        self.assertFalse(any(k.startswith('.assertion.') for k in vars(ns['C'])))
        self.assertEqual(ns['result'], {})

    def test_closure(self):
        exc = fail('''
def outer(x):
    def inner():
        assert x + 1 == 0
    return inner
outer(2)()
''')
        self.assertIn('assert (2 + 1) == 0', exc.details.format())

    def test_comprehension_and_lambda(self):
        for expression in ('[x + 1 for x in range(3)] == []',
                           '{x for x in range(3)} == set()',
                           '{x: x for x in range(3)} == {}',
                           '(lambda x: x + 1)(3) == 0',
                           'all(x < 2 for x in range(3))'):
            with self.subTest(expression=expression):
                exc = fail('assert ' + expression)
                self.assertTrue(exc.details.format())

    def test_repeated_assert_no_stale_captures(self):
        ns = execute('''
errors = []
for value in [True, False]:
    try:
        assert value and (other := True)
    except AssertionError as exc:
        errors.append(exc)
''')
        text = ns['errors'][0].details.format()
        self.assertEqual(text, 'assert (False)')
        details = ns['errors'][0].details
        skipped, = [node for node in details.nodes if node.text == '(other := True)']
        self.assertFalse(skipped.evaluated)
        with self.assertRaises(ValueError):
            skipped.value

    def test_release_on_success_and_expression_error(self):
        class Value:
            pass
        for expression in ('make() is not None', 'make() and explode()'):
            refs = []
            def make():
                value = Value()
                refs.append(weakref.ref(value))
                return value
            def explode():
                raise ValueError
            ns = {'make': make, 'explode': explode}
            execute('def f():\n    try:\n        assert ' + expression +
                    '\n    except ValueError:\n        pass\n'
                    '    return 42\nf()', namespace=ns)
            gc.collect()
            self.assertTrue(all(ref() is None for ref in refs))

    def test_generator_close(self):
        ns = execute('def g():\n    assert (yield 1) == 3\n')
        g = ns['g']()
        self.assertEqual(next(g), 1)
        with self.assertRaises(AssertionError) as caught:
            g.send(2)
        self.assertIn('assert 2 == 3', caught.exception.details.format())
        g = ns['g']()
        next(g)
        g.close()

    def test_await(self):
        ns = execute('''
async def value(): return 4
async def f():
    assert await value() == 5
''')
        with self.assertRaises(AssertionError) as caught:
            asyncio.run(ns['f']())
        self.assertIn('assert 4 == 5', caught.exception.details.format())

    def test_repr_only_at_display(self):
        class Value:
            def __repr__(self):
                raise RuntimeError('broken repr')
        value = Value()
        exc = fail('assert value is None', {'value': value})
        self.assertIn('AssertionError', ''.join(traceback.format_exception(exc)))
        self.assertNotIn('RuntimeError: broken repr', ''.join(traceback.format_exception(exc)))

    def test_exception_subclass_dict_error(self):
        class Error(AssertionError):
            @property
            def __dict__(self):
                raise RuntimeError('dict error')
        self.assertIn('Error: message', ''.join(traceback.format_exception(Error('message'))))

    def test_string_expressions_and_super(self):
        for source in (
            "a = 3\nassert f'{a:04d}' == '0004'",
            "a = 3\nassert t'{a}' is None",
            "class A:\n def value(self): return 1\n"
            "class B(A):\n def f(self):\n  assert super().value() == 2\nB().f()",
        ):
            with self.subTest(source=source):
                exc = fail(source)
                self.assertTrue(exc.details.format())

    def test_builtin_differences(self):
        for source, message in (
            ("assert 'abc' == 'axc'", '  - axc\n  + abc'),
            ('assert [1, 2] == [1, 3]', 'At index 1 diff: 2 != 3'),
            ("assert {'a': 1} == {'a': 2}", "key 'a': 1 != 2"),
            ('assert {1, 2} == {1, 3}', 'only in left'),
        ):
            with self.subTest(source=source):
                exc = fail(source)
                self.assertIn(message, exc.details.format())

    def test_ast_and_marshal(self):
        tree = ast.parse('assert 2 == 3')
        code = compile(tree, '<ast>', 'exec', flags=FLAGS)
        code = marshal.loads(marshal.dumps(code))
        with self.assertRaises(AssertionError) as caught:
            exec(code)
        self.assertTrue(caught.exception._assertion)

    def test_optimized_code_identical(self):
        source = 'def f(x):\n    assert x + 1 == 3\n    return x\n'
        for optimize in (1, 2):
            a = compile(source, 'test', 'exec', optimize=optimize)
            b = compile(source, 'test', 'exec', optimize=optimize, flags=FLAGS)
            self.assertEqual(marshal.dumps(a), marshal.dumps(b))

    def test_plain_assert(self):
        with self.assertRaises(AssertionError) as caught:
            execute('assert 1 == 2', flags=0)
        self.assertFalse(hasattr(caught.exception, '_assertion'))

    def test_traceback_and_clear(self):
        exc = fail('assert 1 == 2')
        summary = traceback.TracebackException.from_exception(exc)
        exc.details = None
        self.assertFalse(hasattr(exc, '_assertion'))
        self.assertIn('assert 1 == 2', ''.join(summary.format()))


class AssertionRenderingTests(unittest.TestCase):
    def explanation(self, source, namespace=None):
        return fail(source, namespace).details.format()

    def test_arithmetic(self):
        self.assertEqual(self.explanation(
            'assert price * quantity == expected',
            {'price': 18, 'quantity': 3, 'expected': 60}),
            'assert (18 * 3) == 60')
        self.assertEqual(self.explanation('x = 2\nassert x + (x := 3) == 6'),
                         'assert (2 + (3)) == 6')

    def test_nested_calls(self):
        self.assertEqual(self.explanation(
            'assert double(len(items)) == 5',
            {'double': lambda x: x * 2, 'items': [1, 2]}),
            'assert 4 == 5\n'
            '  + where 4 = double(2)\n'
            '    + where 2 = len([1, 2])')

    def test_methods(self):
        self.assertEqual(self.explanation("assert name.strip().lower() == 'bob'",
                                          {'name': ' ALICE '}),
            "assert 'alice' == 'bob'\n"
            "  + where 'alice' = 'ALICE'.lower()\n"
            "    + where 'ALICE' = ' ALICE '.strip()\n"
            '  - bob\n'
            '  + alice')

    def test_literals_keywords_and_unicode(self):
        self.assertEqual(self.explanation(
            "assert f('x', *args, x=x, **kw) == 1",
            {'f': lambda *args, **kwargs: 0, 'args': (2,), 'x': 3, 'kw': {'y': 4}}),
            'assert 0 == 1\n'
            "  + where 0 = f('x', *(2,), x=3, **{'y': 4})")
        self.assertEqual(self.explanation('assert α + β == 10', {'α': 2, 'β': 3}),
                         'assert (2 + 3) == 10')

    def test_slice(self):
        self.assertEqual(self.explanation('assert data[start:stop] == []',
                                          {'data': [1, 2], 'start': 0, 'stop': 1}),
            'assert [1] == []\n'
            '  + where [1] = [1, 2][0:1]\n'
            '  sequence lengths: 1 != 0')

    def test_boolean_and_chained_comparisons(self):
        for expression, expected in (
            ('1 == 2 and missing()', '(1 == 2)'),
            ('0 or False', '(0 or False)'),
            ('0 < 5 < 3 < missing()', '5 < 3'),
            ('not (1 == 1)', 'not (1 == 1)'),
            ('0 if True else missing()', '0'),
        ):
            with self.subTest(expression=expression):
                self.assertEqual(self.explanation('assert ' + expression),
                                 f'assert {expected}')

    def test_custom_comparison_replaces_summary(self):
        details = fail('assert 0 < 5 < 3').details
        calls = []
        def compare(op, left, right):
            calls.append((op, left, right))
            return f'custom: {left} {op} {right}\ncomparison detail'
        self.assertEqual(details.format(compare=compare),
                         'assert custom: 5 < 3\n'
                         '  comparison detail')
        self.assertEqual(calls, [('<', 0, 5), ('<', 5, 3)])

    def test_large_diff_is_bounded(self):
        text = self.explanation('assert left == right',
                                {'left': 'a' * 5000, 'right': 'b' * 5000})
        self.assertIn('string diff omitted', text)
        self.assertLess(len(text), 1000)

    def test_framework_explanation_is_not_duplicated(self):
        with assertionlib.formatter(lambda details: 'assert custom difference'):
            error = fail('assert 1 == 2')
        self.assertEqual(''.join(traceback.format_exception_only(error)),
                         'AssertionError: assert custom difference\n')
        self.assertIsNotNone(error.details)
        error.details = None
        self.assertFalse(hasattr(error, '_assertion_explained'))


class PublicAssertionAPITests(unittest.TestCase):
    def test_compact_representations(self):
        expression = 'x + y * z * 2 == x + y + y'
        details = fail('assert ' + expression, {'x': 1, 'y': 2, 'z': 3}).details
        self.assertEqual(repr(details),
                         f'AssertionDetails(expression={expression!r})')
        self.assertEqual(pprint.pformat(details), repr(details))
        comparison, = details.comparisons
        self.assertIs(comparison.expression, details.root)
        self.assertIs(details.root, details.nodes[0])
        self.assertIs(comparison.left, details.root.children[0])
        self.assertIs(comparison.right, details.root.children[1])
        for value in (details.root, comparison):
            for render in (repr, pprint.pformat):
                with self.subTest(value=type(value).__name__, render=render):
                    text = render(value)
                    self.assertEqual(text.count(type(value).__name__ + '('), 1)
                    self.assertIn('truth=False', text)
                    self.assertNotIn('location=', text)
                    self.assertNotIn('children=', text)
        self.assertEqual(pprint.pformat(details.nodes).count('Expression('), 13)
        self.assertNotIn('Expression(', pprint.pformat(details.comparisons))

    def test_representations_do_not_call_captured_values(self):
        class Value:
            def __repr__(self):
                raise RuntimeError('repr called')
            def __bool__(self):
                raise RuntimeError('bool called')
        value = Value()
        details = fail('assert value is None', {'value': value}).details
        for item in (details, details.nodes, details.comparisons):
            repr(item)
            pprint.pformat(item)
        self.assertIs(details.comparisons[0].left.value, value)

    def test_details_are_lazy(self):
        from test.support.script_helper import assert_python_ok
        assert_python_ok('-c', '''
import ast
import sys
assert AssertionError().details is None
exec(compile('assert 1 == 1', '<test>', 'exec', flags=ast.PyCF_ASSERT_DETAILS))
try:
    exec(compile('assert 1 == 2', '<test>', 'exec', flags=ast.PyCF_ASSERT_DETAILS))
except AssertionError as error:
    assert 'assertionlib' not in sys.modules
    details = error.details
    assert details is error.details
    assert 'assertionlib' in sys.modules
    assert '_assertion' not in sys.modules
    assert details.format() == 'assert 1 == 2'
''')

    def test_formatter_default_and_empty_results(self):
        marker = object()
        with assertionlib.formatter(lambda details: None):
            error = fail('assert False, marker', {'marker': marker})
        self.assertEqual(error.args, (marker,))
        self.assertIn('assert False', ''.join(traceback.format_exception_only(error)))
        with assertionlib.formatter(lambda details: ''):
            error = fail('assert False, "message"')
        self.assertEqual(error.args, ('message',))
        self.assertEqual(''.join(traceback.format_exception_only(error)),
                         'AssertionError: message\n')
        with assertionlib.formatter(lambda details: 'outer'):
            with assertionlib.formatter(None):
                self.assertEqual(fail('assert False').args, ())
            self.assertEqual(fail('assert False').args, ('outer',))
        with assertionlib.formatter(lambda details: 'custom'):
            self.assertEqual(fail('assert False, ""').args, ('custom',))

    def test_details_detached_during_construction(self):
        import builtins
        from unittest.mock import patch

        error = fail('assert 1 == 2')
        original_import = builtins.__import__
        def detach(name, *args, **kwargs):
            if name == 'assertionlib':
                error.details = None
            return original_import(name, *args, **kwargs)
        with patch.object(builtins, '__import__', detach):
            self.assertIsNone(error.details)
        self.assertIsNone(error.details)
        self.assertNotIn('_assertion', error.__dict__)

    def test_details_reentrant_construction(self):
        from unittest.mock import patch

        error = fail('assert 1 == 2')
        build = assertionlib._from_payload
        observed = []
        def reenter(payload):
            with patch.object(assertionlib, '_from_payload', build):
                observed.append(error.details)
            return build(payload)
        with patch.object(assertionlib, '_from_payload', reenter):
            details = error.details
        self.assertIs(details, observed[0])
        self.assertIs(details, error.details)

    def test_invalid_formatter_results(self):
        for result in (False, ['text'], 42):
            with self.subTest(result=result):
                with assertionlib.formatter(lambda details: result):
                    with self.assertRaisesRegex(TypeError, 'return str or None') as caught:
                        fail('assert 1 == 2')
                self.assertIsInstance(caught.exception.__context__, AssertionError)
                with self.assertRaisesRegex(TypeError, 'return str or None'):
                    fail('assert 1 == 2').details.format(compare=lambda *args: result)

    def test_empty_comparison_result(self):
        details = fail('assert [1, 2] == [1, 3]').details
        self.assertEqual(details.format(compare=lambda *args: ''),
                         'assert [1, 2] == [1, 3]')

    def test_details_can_only_be_cleared(self):
        error = fail('assert False')
        with self.assertRaisesRegex(TypeError, 'only be set to None'):
            error.details = error.details
        with self.assertRaisesRegex(TypeError, 'only be set to None'):
            error.details = 42
        error.details = None
        error.details = None
        self.assertIsNone(error.details)

    def test_pickle_omits_captures(self):
        for custom in (False, True):
            for protocol in range(pickle.HIGHEST_PROTOCOL + 1):
                with self.subTest(custom=custom, protocol=protocol):
                    callback = (lambda details: 'custom') if custom else None
                    with assertionlib.formatter(callback):
                        error = fail('assert value is None, "message"',
                                     {'value': lambda: None})
                    details = error.details
                    error.extra = 'user attribute'
                    restored = pickle.loads(pickle.dumps(error, protocol))
                    self.assertEqual(restored.args, error.args)
                    self.assertEqual(restored.extra, error.extra)
                    self.assertIsNone(restored.details)
                    self.assertIs(error.details, details)

    def test_expression_graph_and_comparison(self):
        exc = fail('a = 3\nb = 4\nassert a == b and missing()')
        details = exc.details
        self.assertIs(details, exc.details)
        self.assertEqual(details.root.kind, 'and')
        self.assertFalse(details.root.truth)
        self.assertFalse(details.root.children[1].evaluated)
        with self.assertRaises(ValueError):
            details.root.children[1].value
        comparison, = details.comparisons
        self.assertEqual(comparison.operator, '==')
        self.assertEqual(comparison.left.value, 3)
        self.assertEqual(comparison.right.value, 4)
        self.assertIs(comparison.truth, False)
        self.assertEqual(details.location.filename, '<assert-details>')
        self.assertEqual(details.location.lineno, 3)
        self.assertIsNone(AssertionError().details)
        self.assertFalse(hasattr(ValueError(), 'details'))

    def test_distinct_occurrences_and_none(self):
        values = iter([None, 1])
        details = fail('assert f() == f()', {'f': lambda: next(values)}).details
        comparison, = details.comparisons
        self.assertIsNot(comparison.left, comparison.right)
        self.assertTrue(comparison.left.evaluated)
        self.assertIsNone(comparison.left.value)
        self.assertEqual(comparison.right.value, 1)

    def test_nested_control_flow_values(self):
        for expression in ('(a or b) and a', 'not b', 'a if b else b', '0 < b < a < 9'):
            with self.subTest(expression=expression):
                ns = {'a': 0, 'b': 2}
                details = fail('assert ' + expression, ns).details
                self.assertFalse(details.root.truth)
                self.assertEqual(details.root.value, eval(expression, ns))

    def test_callbacks_before_catch_and_message_order(self):
        events = []
        def handler(details):
            events.append('callback')
            self.assertEqual(sys.exception().args, ('original',))
            self.assertFalse(details.root.truth)
            return 'custom explanation'
        with assertionlib.formatter(handler):
            exc = fail('assert events.append("condition"), '
                       '(events.append("message"), "original")[1]', {'events': events})
            self.assertEqual(str(exc), 'original\ncustom explanation')
        self.assertEqual(events, ['condition', 'message', 'callback'])

    def test_nested_scopes_and_recursion(self):
        events = []
        def outer(details):
            events.append('outer')
            fail('assert False')
        def inner(details):
            events.append('inner')
        with assertionlib.formatter(outer):
            fail('assert False')
            with assertionlib.formatter(inner):
                fail('assert False')
            fail('assert False')
        fail('assert False')
        self.assertEqual(events, ['outer', 'inner', 'outer'])
        with self.assertRaises(TypeError):
            with assertionlib.formatter(42):
                pass

    def test_callback_exception_context(self):
        def broken(details):
            raise RuntimeError('callback failed')
        with assertionlib.formatter(broken):
            with self.assertRaisesRegex(RuntimeError, 'callback failed') as caught:
                fail('assert 1 == 2')
        self.assertIsInstance(caught.exception.__context__, AssertionError)
        self.assertIsNotNone(caught.exception.__context__.details)

    def test_dispatch_lookup_exception_context(self):
        import sys
        import types
        from unittest.mock import patch

        def broken(name):
            raise RuntimeError('dispatcher lookup failed')

        module = types.ModuleType('assertionlib')
        module.__getattr__ = broken
        with patch.dict(sys.modules, assertionlib=module):
            with self.assertRaisesRegex(RuntimeError, 'dispatcher lookup failed') as caught:
                fail('assert 1 == 2')
        self.assertIsInstance(caught.exception.__context__, AssertionError)
        self.assertIsNotNone(caught.exception.__context__.details)

    def test_traceback_uses_cached_details(self):
        from unittest.mock import patch

        error = fail('assert 1 == 2')
        details = error.details
        with patch.object(assertionlib, '_from_payload',
                          side_effect=RuntimeError('graph rebuilt')), \
             patch('builtins.__import__', side_effect=RuntimeError('import repeated')):
            self.assertIs(error.details, details)
        with patch.object(assertionlib, '_from_payload',
                          side_effect=RuntimeError('graph rebuilt')):
            self.assertIn('assert 1 == 2', ''.join(traceback.format_exception(error)))
            self.assertIs(error.details, details)

    def test_comparison_values_without_duplicate_captures(self):
        cases = (
            ('(a < b) == True', {'a': 2, 'b': 1}, 'a < b', False),
            ('(a < b < c) == True', {'a': 2, 'b': 1, 'c': 3}, 'a < b < c', False),
            ('(a < b < c) == False', {'a': 1, 'b': 2, 'c': 3}, 'a < b < c', True),
            ('a == (a := 2)', {'a': 1}, 'a', 1),
        )
        for expression, namespace, text, value in cases:
            with self.subTest(expression=expression):
                details = fail('assert ' + expression, namespace).details
                node = next(node for node in details.nodes if node.text == text)
                self.assertTrue(node.evaluated)
                self.assertEqual(node.value, value)

    def test_capture_slots_after_many_parameters(self):
        for count in (2, 14, 16, 260):
            with self.subTest(parameters=count):
                parameters = ', '.join(f'a{i}' for i in range(count))
                namespace = execute(
                    f'def check({parameters}):\n'
                    '    assert (value := a0 + a1) == -1\n')
                with self.assertRaises(AssertionError) as caught:
                    namespace['check'](*range(count))
                comparison, = caught.exception.details.comparisons
                self.assertEqual(comparison.left.value, 1)
                self.assertEqual(comparison.right.value, -1)

    def test_success_does_not_dispatch(self):
        events = []
        with assertionlib.formatter(lambda *args: events.append(args)):
            execute('assert 1 == 1\nassert True')
        self.assertEqual(events, [])

    def test_async_contexts(self):
        async def worker(label):
            events = []
            def callback(details):
                events.append(label)
            with assertionlib.formatter(callback):
                await asyncio.sleep(0)
                fail('assert False')
                await asyncio.sleep(0)
                fail('assert False')
            return events
        async def main():
            return await asyncio.gather(worker('a'), worker('b'))
        self.assertEqual(asyncio.run(main()), [['a', 'a'], ['b', 'b']])

    def test_custom_comparison_formatter(self):
        left, right = object(), object()
        details = fail('assert left == right', locals()).details
        calls = []
        def compare(op, a, b):
            calls.append((op, a, b))
            return 'custom difference'
        text = details.format(compare=compare)
        self.assertIn('custom difference', text)
        self.assertEqual(len(calls), 1)
        self.assertIs(calls[0][1], left)
        self.assertIs(calls[0][2], right)
        default = details.format(compare=lambda *args: None)
        self.assertIn(' == ', default)

    def test_clear_detaches_public_and_private_data(self):
        class Value:
            pass
        def make():
            value = Value()
            ref = weakref.ref(value)
            return fail('assert value is None', {'value': value}), ref
        error, ref = make()
        details = error.details
        error.__traceback__ = None
        error.details = None
        self.assertIsNone(error.details)
        self.assertIsNotNone(ref())
        del details
        gc.collect()
        self.assertIsNone(ref())

    def test_callback_context_copy_recovers(self):
        import contextvars
        contexts = []
        def callback(details):
            contexts.append(contextvars.copy_context())
        with assertionlib.formatter(callback):
            fail('assert False')
        contexts[0].run(fail, 'assert False')
        self.assertEqual(len(contexts), 2)

    def test_callback_task_inherits_formatter(self):
        async def main():
            events, tasks = [], []
            async def later():
                fail('assert False')
            def callback(details):
                events.append(details)
                if len(events) == 1:
                    tasks.append(asyncio.create_task(later()))
            with assertionlib.formatter(callback):
                fail('assert False')
            await tasks[0]
            return len(events)
        self.assertEqual(asyncio.run(main()), 2)

    def test_callback_preserves_exception_chain(self):
        original = ValueError('outer')
        def callback(details):
            raise RuntimeError('callback')
        try:
            raise original
        except ValueError:
            with assertionlib.formatter(callback):
                with self.assertRaises(RuntimeError) as caught:
                    fail('assert False')
        assertion = caught.exception.__context__
        self.assertIsInstance(assertion, AssertionError)
        self.assertIs(assertion.__context__, original)

    def test_nested_not_and_slice_details(self):
        details = fail('assert (not x) == True', {'x': [1]}).details
        node, = [node for node in details.nodes if node.text == 'x']
        self.assertIs(node.truth, True)
        details = fail('assert data[start:stop] == []', {'data': [1, 2], 'start': 0, 'stop': 1}).details
        self.assertTrue(any(node.evaluated and isinstance(node.value, slice)
                            for node in details.nodes))

    def test_comparison_link_truth_does_not_change(self):
        class Result:
            def __init__(self):
                self.truths = iter((False, True))
            def __bool__(self):
                return next(self.truths)
        class Value:
            def __lt__(self, other):
                return Result()
        details = fail('assert ((a < b < c) and False) == True', {'a': Value(), 'b': Value(), 'c': Value()}).details
        links = [c for c in details.comparisons if c.operator == '<']
        self.assertIs(links[0].truth, False)
        self.assertFalse(links[1].evaluated)

    def test_formatter_reuses_object_repr(self):
        calls = []
        class Value:
            def __repr__(self):
                calls.append(1)
                return 'Value()'
        details = fail('assert x == x and False', {'x': Value()}).details
        details.format()
        self.assertEqual(calls, [1])

    @requires_jit_enabled
    def test_tier2_preserves_captured_floats(self):
        from test.test_capi.test_opt import get_all_executors
        for expression in ('(a + b) * c', '(value := (a + b) * c)'):
            with self.subTest(expression=expression):
                ns = execute('def check(a, b, c, expected, n):\n'
                             '    for _ in range(n):\n'
                             f'        assert {expression} == expected\n')
                check = ns['check']
                for _ in range(20):
                    check(2.0, 3.0, 4.0, 20.0, 1000)
                self.assertTrue(list(get_all_executors(check)))
                with self.assertRaises(AssertionError) as caught:
                    check(2.0, 3.0, 4.0, 99.0, 1000)
                details = caught.exception.details
                intermediate, = [node for node in details.nodes if node.text == 'a + b']
                self.assertEqual(intermediate.value, 5.0)
                self.assertEqual(details.comparisons[0].left.value, 20.0)

    def test_variadic_locals_are_visible(self):
        for flags in (0, FLAGS):
            ns = execute('def f(*args, **kwargs):\n    assert args\n    return locals()', flags=flags)
            self.assertEqual(ns['f'](1, a=2), {'args': (1,), 'kwargs': {'a': 2}})

    def test_slot_reuse_and_named_assignment_lifetime(self):
        source = 'def f(a):\n' + '    assert a == a\n' * 100
        ns = execute(source)
        one = execute('def f(a):\n    assert a == a')
        self.assertEqual(ns['f'].__code__.co_nlocals, one['f'].__code__.co_nlocals)
        ns['f'](object())
        ns = execute('''
class Value: pass
def f():
    assert (value := Value()) is not None
    assert value is not None
    return value
result = f()
''')
        self.assertIsInstance(ns['result'], ns['Value'])


class AssertionCLITests(unittest.TestCase):
    def test_cli_cache_separation_and_optimization(self):
        from test.support.script_helper import assert_python_ok
        import tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, 'assert_helper.py').write_text('def check():\n    assert 1 == 2\n')
            source = (
                'import sys, importlib.util\n'
                'sys.path.insert(0, sys.argv[1])\n'
                'import assert_helper as module\n'
                'print(sys.implementation.cache_tag)\n'
                'print(importlib.util.source_from_cache(module.__spec__.cached) == module.__file__)\n'
                'try: module.check()\n'
                'except AssertionError as exc: print(hasattr(exc, "_assertion"))\n'
                'else: print("removed")\n'
            )
            cases = [
                ([], 'False'),
                (['-X', 'assert_details'], 'True'),
                ([], 'False'),
                (['-O', '-X', 'assert_details'], 'removed'),
                (['-OO', '-X', 'assert_details'], 'removed'),
            ]
            tags = set()
            for flags, expected in cases:
                with self.subTest(flags=flags):
                    _, out, _ = assert_python_ok(*flags, '-c', source, directory)
                    tag, roundtrip, result = out.decode().splitlines()
                    tags.add(tag)
                    self.assertEqual(roundtrip, 'True')
                    self.assertEqual(result, expected)
            self.assertEqual(len(tags), 2)

    def test_invalid_cli_options(self):
        from test.support.script_helper import assert_python_failure
        for option in ('assert_details=bad', 'assert_details=events'):
            with self.subTest(option=option):
                _, _, err = assert_python_failure('-X', option, '-c', 'pass')
                self.assertIn(option.split('=')[0].encode(), err)


if __name__ == '__main__':
    unittest.main()
