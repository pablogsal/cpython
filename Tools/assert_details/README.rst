Native assertion diagnostics prototype
======================================

The CPython compiler captures assertion values. The public ``assertionlib`` module
provides expression graphs, formatters, and context-local callbacks.
See ``Doc/library/assertionlib.rst`` for the API contract and compiler flags.

This prototype includes earlier experiments for successful-assertion callbacks
and assertion retention under optimization. The current PEP excludes those
controls. The proposed exception pickle policy and exception context for
internal diagnostic failures still need implementation work.

Build and run
-------------

Build CPython with the normal configure and make commands. Use the release
build for latency measurements::

    ./python -X assert_details Tools/assert_details/demo.py
    ./python -O -X assert_details -X keep_asserts Tools/assert_details/demo.py

The example exits with an AssertionError and a detailed traceback.
The default explanation substitutes captured operands into the assertion.
Calls and attributes add nested ``where`` lines. The structured API retains
all capture data. This includes skipped nodes and recorded truth results.

Use ``-X assert_details=events`` to enable successful-assertion callbacks.
Each command-line mode has a separate bytecode cache tag.

``Tools/assert_details/run.py`` still provides capture for one selected file.
The compiler flags apply to source and AST input. Flags do not affect code
that already exists. Frozen modules retain their original behavior.

Capture and cleanup
-------------------

Each capture uses a compiler-only fast local. Frames initialize these slots
to NULL. A slot remains NULL when short circuit evaluation skips its expression.
The compiler reuses slots between assertions and aliases duplicate comparison
results. Constant true assertions emit no capture code without event mode.

``STORE_FAST_BORROW`` transfers an owned result to a hidden local and returns
a borrowed stack reference. For borrowed inputs, it creates an owned reference.
Assignment-expression subtrees use ``COPY_TO_FAST`` because ordinary local
stores require owned values. ``CLEAR_FAST`` clears initialized or NULL slots
on success and on exceptions. Hidden slots do not appear in ``locals()``.

The compiler captures actual truth results. It does not repeat truth conversion.
Calls, attributes, slices, arithmetic, conditional expressions, assignment
expressions, and containers use the same capture path. Comprehensions and
generator expressions expose their result as one value.

Method-call and builtin-call shortcuts can bypass expression values.
Capture disables those shortcuts where necessary. This still contributes to
the measured overhead. The three new instructions include tier-2 abstract
ownership updates. Build-specific execution checks remain necessary.

An intrinsic builds the payload on failure. The public details object is lazy.
Failure callbacks can replace the exception message before the caller catches it.
With no callback, ``AssertionError.args`` and ``str(error)`` retain their
normal behavior. Successful assertions in ordinary diagnostic mode create no
diagnostic containers and perform no callback lookup.

Event mode performs callback lookup on successful assertions. It constructs
public details only when a successful-assertion callback exists. Its cost is
separate from ordinary diagnostic capture.

Captured references extend object lifetimes and do not freeze mutable state.
Use ``assertionlib.clear_details(error)`` to detach them from an exception.
A retained details object still owns its values. The private capture payload
has no stable serialization contract.

Pytest adapter
--------------

The experimental adapter keeps pytest's import selection, module registration,
comparison hooks, and report code. It replaces its source-to-code entry point.
It enables assertion retention under optimization and partitions caches by
capture mode. It does not promise exact pytest explanation text.

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
Add ``--events`` to measure the separate event mode.
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
