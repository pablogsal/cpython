"""Structured diagnostics and custom explanations for native assertions.

Compile with ast.PyCF_ASSERT_DETAILS to retain values on failure.
Values are live references; capture does not copy mutable objects.
"""

from contextlib import contextmanager
from contextvars import ContextVar
from dataclasses import dataclass, field

__all__ = [
    'AssertionDetails', 'Expression', 'Comparison', 'SourceLocation',
    'formatter',
]

_formatter = ContextVar('assertionlib.formatter', default=None)
_dispatching = ContextVar('assertionlib.dispatching', default=None)


@dataclass(frozen=True, slots=True)
class SourceLocation:
    filename: str
    lineno: int
    col_offset: int
    end_lineno: int
    end_col_offset: int


class _Captured:
    __slots__ = ()

    @property
    def evaluated(self):
        return bool(self._capture)

    @property
    def value(self):
        """The captured result. Raise ValueError if no result is available."""
        if not self._capture:
            raise ValueError('no captured result')
        return self._capture[0]


@dataclass(frozen=True, slots=True, eq=False)
class Expression(_Captured):
    text: str
    kind: str
    location: SourceLocation = field(repr=False)
    children: tuple = field(default=(), repr=False)
    truth: bool | None = None
    _capture: tuple = field(default=(), repr=False)


@dataclass(frozen=True, slots=True, eq=False)
class Comparison(_Captured):
    operator: str
    left: Expression
    right: Expression
    expression: Expression
    truth: bool | None = None
    _capture: tuple = field(default=(), repr=False)

    def __repr__(self):
        return f'{type(self).__name__}(text={self.text!r}, truth={self.truth!r})'

    @property
    def text(self):
        return f'{self.left.text} {self.operator} {self.right.text}'


@dataclass(frozen=True, slots=True, eq=False)
class AssertionDetails:
    expression: Expression
    comparisons: tuple[Comparison, ...]

    def __repr__(self):
        return f'{type(self).__name__}(expression={self.expression.text!r})'

    def format(self, *, compare=None):
        """Return explanation text without a terminal newline.

        compare(op, left, right) returns custom text, or None for the default
        explanation. An empty string suppresses the default difference.
        """
        from _assertion import format_details
        return format_details(self, compare=compare)


def _from_payload(payload):
    records, values, plan, filename = payload
    nodes = tuple(Expression(label, kind, SourceLocation(filename, *span))
                  for label, kind, span, children in plan)
    set_attr = object.__setattr__
    for node, descriptor in zip(nodes, plan):
        set_attr(node, 'children', tuple(nodes[i] for i in descriptor[3]))
    comparisons = []
    last_comparison = {}
    for record, captured in zip(records, values):
        slot, kind, node_id, extra = record
        node = nodes[node_id]
        if kind == 'value':
            set_attr(node, '_capture', captured)
        elif kind == 'comparison':
            op, left_id, right_id = extra
            left, right = nodes[left_id], nodes[right_id]
            comparison = Comparison(op, left, right, node, _capture=captured)
            comparisons.append(comparison)
            if captured:
                last_comparison[node] = comparison
        elif captured:
            if extra is None:
                set_attr(node, 'truth', captured[0])
            else:
                set_attr(last_comparison[node], 'truth', captured[0])

    # Control-flow expressions need not materialize their result in bytecode.
    # Recover their selected result from captured children, without user calls.
    for node in reversed(nodes):
        children = node.children
        selected = None
        if node.kind in ('and', 'or'):
            stop = node.kind == 'or'
            for child in children:
                if not child.evaluated:
                    break
                selected = child
                if child.truth is stop:
                    break
        elif node.kind == 'conditional' and len(children) == 3:
            condition, body, otherwise = children
            if condition.truth is not None:
                selected = body if condition.truth else otherwise
        elif node.kind == 'not' and children and children[0].truth is not None:
            result = not children[0].truth
            set_attr(node, '_capture', (result,))
            set_attr(node, 'truth', result)
        elif node.kind == 'compare':
            selected = last_comparison.get(node)
            if selected is not None:
                if node.truth is not None and selected.truth is None:
                    set_attr(selected, 'truth', node.truth)
        if selected is not None:
            if not node._capture:
                set_attr(node, '_capture', selected._capture)
            if node.truth is None:
                set_attr(node, 'truth', selected.truth)
    root = nodes[0]
    set_attr(root, 'truth', False)
    return AssertionDetails(root, tuple(comparisons))


@contextmanager
def formatter(callback):
    """Use callback(details) to format failed assertions in this context.

    Return text for a custom explanation, or None for the default explanation.
    Pass None to suspend a surrounding formatter. Exit restores it.
    """
    if callback is not None and not callable(callback):
        raise TypeError('assertion formatter must be callable or None')
    token = _formatter.set(callback)
    try:
        yield
    finally:
        _formatter.reset(token)


def _dispatch(error):
    callback = _formatter.get()
    if callback is None:
        return
    active = _dispatching.get()
    if active is not None and active[0]:
        return
    # Context copies share this marker until the callback returns.
    active = [True]
    token = _dispatching.set(active)
    try:
        text = callback(error.details)
        if text is not None:
            if not isinstance(text, str):
                raise TypeError('assertion formatter must return str or None')
            if error.args:
                message = str(error)
                if message:
                    text = message + '\n' + text if text else message
            error.args = (text,)
            error._assertion_explained = True
    finally:
        active[0] = False
        _dispatching.reset(token)
