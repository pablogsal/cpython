"""Structured diagnostics and callbacks for native assertions.

Compile with ast.PyCF_ASSERT_DETAILS to capture values on failure. Add
ast.PyCF_ASSERT_EVENTS for successful-assertion callbacks. Values are live
references; capture does not copy mutable objects.
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
    passed: bool
    _payload: tuple = field(repr=False)


def _from_payload(payload):
    text, records, values, plan, filename, passed = payload
    nodes = tuple(Expression(i, label, kind, SourceLocation(filename, *span))
                  for i, (label, kind, span, children) in enumerate(plan))
    set_attr = object.__setattr__
    for node, descriptor in zip(nodes, plan):
        set_attr(node, 'children', tuple(nodes[i] for i in descriptor[3]))
    comparisons = []
    by_link = {}
    by_node = {}
    for record, captured in zip(records, values):
        slot, label, kind, span, extra, node_id = record
        node = nodes[node_id]
        if kind == 'value':
            set_attr(node, '_capture', captured)
            set_attr(node, 'evaluated', bool(captured))
        elif kind == 'comparison':
            left_slot, right_slot, op, left_id, right_id = extra
            comparison = Comparison(op, nodes[left_id], nodes[right_id],
                                    node, label, bool(captured), _capture=captured)
            comparisons.append(comparison)
            by_link[node_id, extra] = comparison
            by_node.setdefault(node_id, []).append(comparison)
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
            links = by_node.get(node.id, ())
            evaluated = [link for link in links if link.evaluated]
            if evaluated:
                selected = evaluated[-1]
                if node.truth is not None and selected.truth is None:
                    set_attr(selected, 'truth', node.truth)
        if selected is not None:
            if not node._capture:
                set_attr(node, '_capture', selected._capture)
            set_attr(node, 'evaluated', selected.evaluated)
            if node.truth is None:
                set_attr(node, 'truth', selected.truth)
    root = nodes[0]
    set_attr(root, 'truth', passed)
    set_attr(root, 'evaluated', True)
    return AssertionDetails(text, root.location, root, nodes,
                            tuple(comparisons), passed, payload)


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
    from _assertion import clear_details as clear_payload
    clear_payload(error)


def format_details(details, *, compare=None):
    """Return explanation lines, including their terminal newlines.

    compare(op, left, right) returns custom lines without terminal newlines,
    or None to request the default explanation. Formatter errors propagate.
    """
    if not isinstance(details, AssertionDetails):
        raise TypeError('expected AssertionDetails')
    from _assertion import _format_details
    return _format_details(details, compare=compare)


@contextmanager
def handlers(*, on_failure=None, on_pass=None):
    """Install synchronous callbacks in the current execution context.

    Nested scopes restore their previous callbacks. Context copies, including
    new asyncio tasks, inherit these callbacks. Thread context rules apply.
    """
    for callback in (on_failure, on_pass):
        if callback is not None and not callable(callback):
            raise TypeError('assertion callbacks must be callable or None')
    token = _handlers.set((on_failure, on_pass))
    try:
        yield
    finally:
        _handlers.reset(token)


def _dispatch(error, payload):
    active = _dispatching.get()
    if active is not None and active[0]:
        return
    callbacks = _handlers.get()
    callback = callbacks[error is None]
    active = [True]
    token = _dispatching.set(active)
    try:
        details = _from_payload(payload) if error is None else get_details(error)
        if error is None:
            callback(details)
        else:
            callback(error, details)
    finally:
        active[0] = False
        _dispatching.reset(token)
