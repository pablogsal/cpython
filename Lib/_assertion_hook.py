"""Default ``sys.__assertion_hook__`` implementation.

The compiler emits a call to ``sys.__assertion_hook__`` on every failed
``assert`` statement (see ``Python/asserthook.c`` and the
``INTRINSIC_FORMAT_ASSERT`` intrinsic).  When the slot is ``None`` (its
initial value) the original ``assert`` semantics are preserved.  Programs
that want pytest-style diffs can opt in with::

    import _assertion_hook
    _assertion_hook.install()

This module is intentionally written in pure Python -- the heavy lifting of
capturing intermediate values is done in C by the compiler/codegen, so the
hook only needs to format what it receives.
"""

from __future__ import annotations

import difflib
import reprlib
import sys
from typing import Any, Sequence

__all__ = ["install", "uninstall", "format_assertion", "default_hook"]


_MAX_REPR = 240


def _safe_repr(value: Any) -> str:
    """Like ``repr(value)`` but tolerant of broken ``__repr__`` methods and
    truncates very long results so a single failed assert can't flood the
    terminal."""
    try:
        r = reprlib.Repr()
        r.maxstring = _MAX_REPR
        r.maxother = _MAX_REPR
        r.maxlist = 12
        r.maxtuple = 12
        r.maxdict = 12
        return r.repr(value)
    except Exception as exc:
        return f"<unrepresentable {type(value).__name__}: {exc!r}>"


def _diff_lines(a: str, b: str) -> list[str]:
    """Return a unified diff between two multi-line strings, with no header."""
    a_lines = a.splitlines(keepends=False)
    b_lines = b.splitlines(keepends=False)
    diff = list(
        difflib.unified_diff(a_lines, b_lines, lineterm="", n=2)
    )
    # Drop the "--- / +++" header difflib emits for unified_diff.
    return diff[2:] if len(diff) >= 2 else diff


def format_assertion(
    source_strs: Sequence[str],
    values: Sequence[Any],
    msg: object,
    expr_source: str,
) -> str:
    """Format a pytest-style explanation for a failed assertion.

    Receives the data the compiler captured: the source representation of
    each "interesting" sub-expression in the test (``source_strs``), the
    runtime ``values`` they evaluated to on the failure path, the optional
    user message and the source of the whole test expression.
    """
    lines = [f"assert {expr_source}"]
    if msg is not None:
        lines.append(f"  message: {msg}")

    # Pair up each captured sub-expression with its value.
    for src, value in zip(source_strs, values):
        lines.append(f"  where {src} = {_safe_repr(value)}")

    # Special case binary equality: show a diff for str/bytes/sequences.
    if len(values) == 2 and " == " in expr_source:
        left, right = values
        try:
            equal = left == right
        except Exception:
            equal = True  # if comparison itself errors, skip the diff
        if not equal:
            if isinstance(left, str) and isinstance(right, str):
                diff = _diff_lines(left, right)
                if diff:
                    lines.append("  diff:")
                    lines.extend(f"    {line}" for line in diff)
            elif (
                isinstance(left, (list, tuple))
                and isinstance(right, (list, tuple))
                and type(left) is type(right)
            ):
                # Show first differing index, similar to pytest.
                for i, (a, b) in enumerate(zip(left, right)):
                    if a != b:
                        lines.append(
                            f"  first differing item at index {i}: "
                            f"{_safe_repr(a)} != {_safe_repr(b)}"
                        )
                        break
                if len(left) != len(right):
                    lines.append(
                        f"  lengths differ: {len(left)} vs {len(right)}"
                    )

    return "\n".join(lines)


def default_hook(
    source_strs: tuple[str, ...],
    values: tuple[Any, ...],
    msg: object,
    expr_source: str,
) -> str:
    """Reference implementation suitable for use as ``sys.__assertion_hook__``."""
    return format_assertion(source_strs, values, msg, expr_source)


_previous: object = None


def install() -> None:
    """Install ``default_hook`` as ``sys.__assertion_hook__``."""
    global _previous
    _previous = sys.__assertion_hook__
    sys.__assertion_hook__ = default_hook


def uninstall() -> None:
    """Restore the previous value of ``sys.__assertion_hook__``."""
    global _previous
    sys.__assertion_hook__ = _previous
    _previous = None
