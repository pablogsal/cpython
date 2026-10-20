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


@dataclass(frozen=True, slots=True, eq=False)
class Expression:
    id: int
    text: str
    kind: str
    location: SourceLocation
    children: tuple = ()
    evaluated: bool = False
    truth: bool | None = None
    _capture: tuple = field(default=(), repr=False)

    @property
    def value(self):
        """The captured result. Raise ValueError if no result is available."""
        if not self._capture:
            raise ValueError('expression has no captured result')
        return self._capture[0]


@dataclass(frozen=True, slots=True, eq=False)
class Comparison:
    operator: str
    left: Expression
    right: Expression
    expression: Expression
    text: str
    evaluated: bool
    truth: bool | None = None
    _capture: tuple = field(default=(), repr=False)

    @property
    def value(self):
        if not self._capture:
            raise ValueError('comparison was not evaluated')
        return self._capture[0]


@dataclass(frozen=True, slots=True, eq=False)
class AssertionDetails:
    expression: str
    location: SourceLocation
    root: Expression
    nodes: tuple[Expression, ...]
    comparisons: tuple[Comparison, ...]

    def format(self, *, compare=None):
        """Return explanation text without a terminal newline.

        compare(op, left, right) returns custom text, or None for the default
        explanation. An empty string suppresses the default difference.
        """
        from _assertion import format_details
        return format_details(self, compare=compare)


def _from_payload(payload):
    records, values, plan, filename = payload
    nodes = tuple(Expression(i, label, kind, SourceLocation(filename, *span))
                  for i, (label, kind, span, children) in enumerate(plan))
    set_attr = object.__setattr__
    for node, descriptor in zip(nodes, plan):
        set_attr(node, 'children', tuple(nodes[i] for i in descriptor[3]))
    comparisons = []
    by_link = {}
    last_comparison = {}
    for record, captured in zip(records, values):
        slot, kind, node_id, extra = record
        node = nodes[node_id]
        if kind == 'value':
            set_attr(node, '_capture', captured)
            set_attr(node, 'evaluated', bool(captured))
        elif kind == 'comparison':
            op, left_id, right_id = extra
            left, right = nodes[left_id], nodes[right_id]
            comparison = Comparison(op, left, right, node,
                                    f'{left.text} {op} {right.text}',
                                    bool(captured), _capture=captured)
            comparisons.append(comparison)
            by_link[node_id, extra] = comparison
            if captured:
                last_comparison[node_id] = comparison
        elif captured:
            if extra is None:
                set_attr(node, 'truth', captured[0])
            else:
                set_attr(by_link[node_id, extra], 'truth', captured[0])

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
            set_attr(node, 'evaluated', True)
        elif node.kind == 'compare':
            selected = last_comparison.get(node.id)
            if selected is not None:
                if node.truth is not None and selected.truth is None:
                    set_attr(selected, 'truth', node.truth)
        if selected is not None:
            if not node._capture:
                set_attr(node, '_capture', selected._capture)
            set_attr(node, 'evaluated', selected.evaluated)
            if node.truth is None:
                set_attr(node, 'truth', selected.truth)
    root = nodes[0]
    set_attr(root, 'truth', False)
    set_attr(root, 'evaluated', True)
    return AssertionDetails(root.text, root.location, root, nodes,
                            tuple(comparisons))


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
