:mod:`!assertionlib` --- Native assertion diagnostics
===================================================

.. module:: assertionlib
   :synopsis: Structured values and callbacks for native assertions.

.. versionadded:: 3.16

This experimental module exposes values from native assertion capture.
The compiler preserves expression order and short circuit behavior.
Capture does not repeat expression evaluation or truth conversion.

Enable capture
--------------

Compile source with :data:`ast.PyCF_ASSERT_DETAILS`::

    import ast
    code = compile(source, filename, "exec",
                   flags=ast.PyCF_ASSERT_DETAILS, dont_inherit=True)

The flag applies to nested code in that compilation.
It does not change code that already exists or code in imported modules.
Use ``python -X assert_details`` to enable capture for source compilation
throughout the process. Frozen modules and previously compiled code objects retain
their original behavior.

Optimization levels 1 and 2 remove assertions, including their diagnostics.
The C compiler flag is ``PyCF_ASSERT_DETAILS`` in ``PyCompilerFlags.cf_flags``.
The compiler does not inherit this flag through function calls.
The command-line modes use distinct bytecode cache
tags. A custom loader must also distinguish its compiler flags in its cache.

Read and format details
-----------------------

.. function:: get_details(error)

   Return an :class:`AssertionDetails` object, or ``None`` when the exception
   has no native assertion data. Repeated calls return the same object while
   the exception retains the same capture payload.

.. function:: format_details(details, *, compare=None)

   Return explanation lines with terminal newlines. The default formatter
   substitutes captured operands into a compact assertion explanation.
   Calls and attributes use nested ``where`` lines. A chained comparison
   shows its first failed link. Skipped expressions remain available in
   the graph but do not appear in the explanation.
   Representations and differences for builtin objects have size limits.

   For example, ``assert price * quantity == expected`` with values 18, 3,
   and 60 produces::

       assert (18 * 3) == 60

   The optional ``compare(op, left, right)`` callback receives the operator
   string and both operand objects. It returns custom lines without terminal
   newlines, or ``None`` to request the default explanation. The first custom
   line replaces the comparison summary. Additional lines supply detail.
   An empty result retains the summary and suppresses the default difference.

   The formatter calls it for each evaluated comparison. Callback errors propagate.

.. function:: clear_details(error)

   Detach the capture payload and public details object from the exception.
   A details object that another consumer retains still owns its values.

.. class:: AssertionDetails

   An immutable description of one failed assertion. Its attributes are:

   * ``expression`` and ``location``
   * ``root`` and ``nodes``
   * ``comparisons``

   ``nodes`` and ``comparisons`` are tuples. ``root`` identifies
   the expression at the top of the graph.

.. class:: Expression

   An immutable expression node with ``id``, ``text``, ``kind``, ``location``,
   ``children``, ``evaluated``, ``value``, and ``truth`` attributes.
   Node IDs are unique within one assertion. Repeated source expressions
   have distinct nodes. Children are expression nodes in a tuple.

   ``value`` returns the captured result. It raises :exc:`ValueError` if no
   result is available. An evaluated value of ``None`` differs from a skipped
   expression. ``truth`` is the recorded truth result, or ``None`` when no
   truth result is available. It does not call ``bool(value)``.

.. class:: Comparison

   An immutable comparison record with ``operator``, ``left``, ``right``,
   ``expression``, ``text``, ``evaluated``, ``value``, and ``truth`` attributes.
   ``left`` and ``right`` identify operand nodes. A chained comparison has
   one record for each comparison link. This includes skipped links.

.. class:: SourceLocation

   An immutable location with ``filename``, ``lineno``, ``col_offset``,
   ``end_lineno``, and ``end_col_offset`` attributes. Columns use AST byte
   offsets. Expression text comes from the AST unparser and can differ from
   the original source text.

Callbacks
---------

.. function:: handlers(*, on_failure=None)

   Return a context manager that installs synchronous callbacks in the
   current execution context. Nested scopes restore their previous callbacks.
   Context copies inherit the callbacks. New asyncio tasks also inherit them.
   Normal thread context inheritance rules apply.

   ``on_failure(error, details)`` runs after assertion message evaluation,
   before the exception reaches the caller. The callback can change
   ``error.args`` to supply a library-specific explanation.

   Recursive assertions inside a callback do not invoke another callback.
   If a callback or diagnostic support raises, its error has the assertion as its context.
   The assertion error retains any earlier exception context.

Example::

    import assertionlib

    def explain(error, details):
        error.args = ("".join(assertionlib.format_details(details)).strip(),)

    with assertionlib.handlers(on_failure=explain):
        exec(code)

Limits
------

Captured objects remain live references. Capture extends their lifetimes
and can delay destructors. It does not freeze mutable state.
Comprehensions and generator expressions expose their result as one value.
Capture does not record their iteration history or the internal execution
of called functions.

Exception serialization has no special capture policy in this prototype.
A consumer must detach captures before it serializes an exception with
arbitrary operand objects. For remote reports, send formatted text.
