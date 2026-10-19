"""Experimental pytest adapter. Load with ``-p pytest_native``.

This adapter replaces pytest's compiler entry point. Its import selection,
plugin hooks, and report machinery remain in pytest. It requires this CPython
prototype and uses private pytest interfaces only at the loader boundary.
"""

import ast
import importlib.util
import sys

import assertionlib
import pytest
from _pytest.assertion import rewrite, truncate

_original_compile = rewrite._rewrite_test
_original_tail = rewrite.PYC_TAIL


def _compile_test(path, config):
    source = path.read_bytes()
    flags = ast.PyCF_ASSERT_DETAILS | ast.PyCF_KEEP_ASSERTS
    if config.getini('enable_assertion_pass_hook'):
        flags |= ast.PyCF_ASSERT_EVENTS
    code = compile(source, str(path), 'exec', flags=flags, dont_inherit=True)
    return importlib.util.source_hash(source), code


@pytest.hookimpl(tryfirst=True)
def pytest_load_initial_conftests(early_config):
    events = early_config.getini('enable_assertion_pass_hook')
    mode = 'events' if events else 'details'
    rewrite._rewrite_test = _compile_test
    rewrite.PYC_TAIL = (
        f'.{sys.implementation.cache_tag}-pytest-native-v1-{mode}'
        f'-opt{sys.flags.optimize}{rewrite.PYC_EXT}'
    )


def pytest_unconfigure(config):
    rewrite._rewrite_test = _original_compile
    rewrite.PYC_TAIL = _original_tail


def _message(error, details, compare=None):
    explanation = ''.join(assertionlib.format_details(details, compare=compare)).strip()
    if error.args:
        explanation = f'{str(error)}\n{explanation}'
    error.args = (explanation,)
    # Pytest displays this explanation through format_exception_only().
    # Keep captures for plugins, but omit a second, default explanation.
    error._assertion_explained = True


@pytest.hookimpl(wrapper=True)
def pytest_collection(session):
    with assertionlib.handlers(on_failure=_message):
        return (yield)


@pytest.hookimpl(wrapper=True)
def pytest_runtest_protocol(item):
    def compare(op, left, right):
        results = item.ihook.pytest_assertrepr_compare(
            config=item.config, op=op, left=left, right=right,
        )
        for result in results:
            if result:
                lines = truncate.materialize_with_truncation(result, item.config)
                if lines:
                    return [line.replace('\n', '\\n') for line in lines]
        return None

    def failure(error, details):
        _message(error, details, compare)

    def passed(details):
        explanation = ''.join(assertionlib.format_details(details, compare=compare)).strip()
        item.ihook.pytest_assertion_pass(
            item=item, lineno=details.location.lineno,
            orig=details.expression, expl=explanation,
        )

    on_pass = None
    if (item.config.getini('enable_assertion_pass_hook') and
            item.ihook.pytest_assertion_pass.get_hookimpls()):
        on_pass = passed
    with assertionlib.handlers(on_failure=failure, on_pass=on_pass):
        return (yield)
