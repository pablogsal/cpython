Native assertion diagnostics prototype
======================================

The CPython compiler captures assertion values. The public ``assertionlib`` module
provides expression graphs, formatters, and context-local callbacks.
See ``Doc/library/assertionlib.rst`` for the API contract and compiler flags.

The proposed exception pickle policy still needs implementation work.

Build and run
-------------

Build CPython with the normal configure and make commands. Use the release
build for latency measurements::

    ./python -X assert_details Tools/assert_details/demo.py

The example exits with an AssertionError and a detailed traceback.
The default explanation substitutes captured operands into the assertion.
Calls and attributes add nested ``where`` lines. The structured API retains
all capture data. This includes skipped nodes and recorded truth results.

Capture is off by default. ``-O`` and ``-OO`` remove assertions.
Capture uses a separate bytecode cache tag.

``Tools/assert_details/run.py`` still provides capture for one selected file.
The compiler flags apply to source and AST input. Flags do not affect code
that already exists. Frozen modules retain their original behavior.

Capture and cleanup
-------------------

Each capture uses a compiler-only fast local. Frames initialize these slots
to NULL. A slot remains NULL when short circuit evaluation skips its expression.
The compiler reuses slots between assertions. Comparison nodes use the result
of their last evaluated link. Constant true assertions emit no capture code.

Capture uses ``STORE_FAST slot; LOAD_FAST slot``. Cleanup uses
``LOAD_FAST_AND_CLEAR slot; POP_TOP`` on success and on exceptions.
These existing instructions also handle unset slots.
Hidden slots do not appear in ``locals()``.

The constant plan stores expression text, source spans, and child nodes once.
Each capture record identifies a slot, a kind, and a node.
Comparison records also identify the operator and operand nodes.

The compiler captures actual truth results. It does not repeat truth conversion.
Calls, attributes, slices, arithmetic, conditional expressions, assignment
expressions, and containers use the same capture path. Comprehensions and
generator expressions expose their result as one value.

Method-call and builtin-call shortcuts can bypass expression values.
Capture disables those shortcuts where necessary. This still contributes to
the measured overhead. The implementation adds no opcodes or optimizer rules.

An intrinsic builds the payload on failure. The public details object is lazy.
Failure callbacks can replace the exception message before the caller catches it.
With no callback, ``AssertionError.args`` and ``str(error)`` retain their
normal behavior. Successful assertions in ordinary diagnostic mode create no
diagnostic containers and perform no callback lookup.

Captured references extend object lifetimes and do not freeze mutable state.
Use ``assertionlib.clear_details(error)`` to detach them from an exception.
A retained details object still owns its values. The private capture payload
has no stable serialization contract.

Pytest adapter
--------------

The experimental adapter keeps pytest's import selection, module registration,
comparison hooks, and report code. It replaces its source-to-code entry point.
It compiles selected modules with ``optimize=0`` and uses a separate cache tag.
It supports failure callbacks. It does not promise exact pytest explanation text.

Add this directory, the pytest source directory, and its dependencies to
``PYTHONPATH``. Then run::

    ./python -m pytest -p pytest_native path/to/tests

Exercise the adapter and its hooks::

    ./python Tools/assert_details/pytest_smoke.py \
        --pytest-source /path/to/pytest \
        --pytest-deps /path/to/site-packages \
        --output /tmp/assert-pytest

Measurements
------------

Run the microbenchmarks::

    ./python Tools/assert_details/benchmark.py \
        --output /tmp/assert-results --iterations 1000 --cpu 17

For a pytest comparison, pass ``--pytest-source`` and ``--pytest-deps``.
Add ``--callbacks`` to measure failure callback latency.
The benchmark uses one executable for all modes. It warms each function,
randomizes mode order, and reports raw samples, medians, and median absolute
deviations. Runtime includes loop and harness overhead.

Run without ``--iterations`` to measure one assertion per function call.
This also gives bytecode sizes for the smallest function in each case.
Record the compiler flags, CPU, and host load with every result.
Use the same CPU and build options for comparisons between interpreters.

Measure whole-module size and compile latency::

    ./python Tools/assert_details/corpus.py --output /tmp/assert-corpus.json

The corpus includes tracked production library modules with assertions.
Its bytecode and marshal totals include code without assertions.

Checks
------

Run the focused checks::

    ./python -m test test_assert_details

Use a debug build for reference-leak checks. Run the compiler, traceback,
frame, import, generator, coroutine, and marshal suites after changes to
capture or cleanup. Run the C import checks after cache-tag changes.
