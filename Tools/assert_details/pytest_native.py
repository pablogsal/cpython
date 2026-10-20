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
    code = compile(source, str(path), 'exec', flags=ast.PyCF_ASSERT_DETAILS,
                   dont_inherit=True, optimize=0)
    return importlib.util.source_hash(source), code


@pytest.hookimpl(tryfirst=True)
def pytest_load_initial_conftests(early_config):
    if early_config.getini('enable_assertion_pass_hook'):
        raise pytest.UsageError('native assertion capture supports failure callbacks only')
    rewrite._rewrite_test = _compile_test
    rewrite.PYC_TAIL = (
        f'.{sys.implementation.cache_tag}-pytest-native-v2{rewrite.PYC_EXT}'
    )


def pytest_unconfigure(config):
    rewrite._rewrite_test = _original_compile
    rewrite.PYC_TAIL = _original_tail


@pytest.hookimpl(wrapper=True)
def pytest_collection(session):
    with assertionlib.formatter(lambda details: details.format()):
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
                    return '\n'.join(line.replace('\n', '\\n') for line in lines)
        return None

    with assertionlib.formatter(lambda details: details.format(compare=compare)):
        return (yield)
