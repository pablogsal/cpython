"""Structured diagnostics and callbacks for native assertions.

Compile with ast.PyCF_ASSERT_DETAILS to retain values on failure.
Values are live references; capture does not copy mutable objects.
"""

from contextlib import contextmanager
from contextvars import ContextVar
from dataclasses import dataclass, field

__all__ = [
    'AssertionDetails', 'Expression', 'Comparison', 'SourceLocation',
    'get_details', 'clear_details', 'format_details', 'handlers',
]

_handlers = ContextVar('assertionlib.handlers', default=None)
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
    _payload: tuple = field(repr=False)


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
                            tuple(comparisons), payload)


def get_details(error):
    """Return native assertion details, or None for an ordinary exception."""
    if not isinstance(error, AssertionError):
        return None
    payload = error.__dict__.get('_assertion')
    if payload is None:
        return None
    details = error.__dict__.get('_assertion_details')
    if details is None or details._payload is not payload:
        details = _from_payload(payload)
        error.__dict__['_assertion_details'] = details
    return details


def clear_details(error):
    """Detach captured references from an exception.

    A separately retained AssertionDetails object still owns its values.
    """
    error.__dict__.pop('_assertion', None)
    error.__dict__.pop('_assertion_details', None)
    error.__dict__.pop('_assertion_explained', None)


def format_details(details, *, compare=None):
    """Return explanation lines, including their terminal newlines.

    compare(op, left, right) returns custom lines without terminal newlines,
    or None to request the default explanation. Formatter errors propagate.
    """
    if not isinstance(details, AssertionDetails):
        raise TypeError('expected AssertionDetails')
    from _assertion import format_details
    return format_details(details, compare=compare)


@contextmanager
def handlers(*, on_failure=None):
    """Install synchronous callbacks in the current execution context.

    Nested scopes restore their previous callbacks. Context copies, including
    new asyncio tasks, inherit these callbacks. Thread context rules apply.
    """
    if on_failure is not None and not callable(on_failure):
        raise TypeError('assertion callback must be callable or None')
    token = _handlers.set(on_failure)
    try:
        yield
    finally:
        _handlers.reset(token)


def _dispatch(error):
    callback = _handlers.get()
    if callback is None:
        return
    active = _dispatching.get()
    if active is not None and active[0]:
        return
    # Context copies share this marker until the callback returns.
    active = [True]
    token = _dispatching.set(active)
    try:
        callback(error, get_details(error))
    finally:
        active[0] = False
        _dispatching.reset(token)
