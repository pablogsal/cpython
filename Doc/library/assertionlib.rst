:mod:`!assertionlib` --- Native assertion diagnostics
===================================================

.. module:: assertionlib
   :synopsis: Captured assertion values and custom explanations.

.. versionadded:: 3.16

Run Python with ``-X assert_details`` to capture assertion values.
The standard traceback then shows an explanation. Ordinary code needs no
import or callback::

    price, quantity, expected = 18, 3, 60
    assert price * quantity == expected

The explanation is::

    assert (18 * 3) == 60

Capture preserves expression order and short circuit behavior.
It does not repeat expression evaluation or truth conversion.

Read a caught assertion
-----------------------

:attr:`AssertionError.details` returns the captured :class:`AssertionDetails`,
or ``None`` when the exception has no capture data. The first access builds
and caches the graph. Later accesses return the same object::

    try:
        assert price * quantity == expected
    except AssertionError as error:
        if error.details is not None:
            print(error.details.format())
            comparison = error.details.comparisons[0]
            print(comparison.left.value)   # 54
            print(comparison.right.value)  # 60

.. class:: AssertionDetails

   An immutable description of a failed assertion. Its attributes are
   ``expression``, ``location``, ``root``, ``nodes``, and ``comparisons``.
   ``nodes`` and ``comparisons`` are tuples. ``root`` identifies the top
   expression node.

   The representation shows only the assertion text::

       AssertionDetails(expression='price * quantity == expected')

   Node and comparison representations omit graph links, locations, and
   captured values. These attributes remain available for inspection.

   .. method:: format(*, compare=None)

      Return explanation text without a terminal newline. This method does
      not produce output or invoke the scoped formatter. It can call user
      ``__repr__`` methods. Representations and built-in differences have
      size limits.

      The optional ``compare(op, left, right)`` callback receives the operator
      string and captured operands for each evaluated comparison.
      Return ``None`` for the default explanation, or a string for custom text.
      The first line replaces the comparison summary. Later lines supply detail.
      An empty string keeps the summary and suppresses the default difference.
      Any other return type raises :exc:`TypeError`. Callback errors propagate.

Customize an explanation
------------------------

.. function:: formatter(callback)

   Return a context manager that installs a synchronous formatter in the
   current execution context. The callback receives one :class:`AssertionDetails`
   object and returns a string or ``None``::

       import assertionlib

       def explain(details):
           return "Order mismatch:\n" + details.format()

       with assertionlib.formatter(explain):
           assert price * quantity == expected, "checkout failed"

   This produces::

       AssertionError: checkout failed
       Order mismatch:
       assert (18 * 3) == 60

   The callback runs after assertion message evaluation and before the exception
   reaches its caller. A string supplies the explanation. Python prefixes the
   original message, if present, and sets ``error.args`` to that combined text.
   The traceback displays it once. An empty string omits the explanation.
   ``None`` retains the original arguments and default traceback behavior.
   Any other return type raises :exc:`TypeError`.

   The callback cannot suppress the assertion. If it raises, its error has
   the assertion as its context. This also applies to errors in diagnostic
   support. The assertion retains any earlier exception context.

   Nested scopes replace the formatter. Pass ``None`` to suspend a surrounding
   formatter. Exit restores the previous formatter. Context copies and new
   asyncio tasks inherit it. Normal thread context inheritance rules apply.
   Recursive assertions within a formatter do not invoke another formatter.

   This function changes the explanation. It does not enable compiler capture.

For domain-specific comparisons, use the standard formatter with a comparison
callback::

    def compare_totals(op, left, right):
        if op == "==" and type(left) is int and type(right) is int:
            return f"totals differ by {right - left}"
        return None

    def explain(details):
        return details.format(compare=compare_totals)

    with assertionlib.formatter(explain):
        assert price * quantity == expected

Expression data
---------------

.. class:: Expression

   An immutable node with ``id``, ``text``, ``kind``, ``location``, ``children``,
   ``evaluated``, ``value``, and ``truth`` attributes. IDs are unique within one
   assertion. Repeated source expressions have distinct nodes. Children form
   a tuple of expression nodes.

   Check ``evaluated`` before access to ``value``. An unavailable value raises
   :exc:`ValueError`. An evaluated value of ``None`` differs from a skipped
   expression. ``truth`` is the recorded truth result, or ``None`` when
   unavailable. It does not call ``bool(value)``.

.. class:: Comparison

   An immutable record with ``operator``, ``left``, ``right``, ``expression``,
   ``text``, ``evaluated``, ``value``, and ``truth`` attributes. ``left`` and
   ``right`` identify operand nodes. A chain has one record for each link,
   including skipped links. ``value`` raises :exc:`ValueError` when unavailable.

.. class:: SourceLocation

   An immutable location with ``filename``, ``lineno``, ``col_offset``,
   ``end_lineno``, and ``end_col_offset`` attributes. Columns use AST byte
   offsets. The AST unparser supplies expression text, which can differ from
   the original source.

Compile selected code
---------------------

Libraries can enable capture for selected source or AST input::

    import ast
    code = compile(source, filename, "exec",
                   flags=ast.PyCF_ASSERT_DETAILS, dont_inherit=True)

The flag applies to nested code in that compilation. It does not affect
imported modules or code that already exists. ``python -X assert_details``
enables capture for source compilation throughout the process. Frozen modules
retain their original behavior. Optimization levels 1 and 2 remove assertions.

The C flag is ``PyCF_ASSERT_DETAILS`` in ``PyCompilerFlags.cf_flags``.
The compiler does not inherit it through function calls. The command-line
option uses a separate bytecode cache tag. Custom loaders must also distinguish
compiler flags in their caches.

Object lifetime
---------------

Captured values are live references. Capture can delay destructors and does
not freeze mutable state. Comprehensions and generators expose their result
as one value. The graph does not record their iteration history or called
functions' internal execution.

Normal use needs no explicit cleanup. The exception and any retained details
objects own the captured references. For saved reports, retain formatted text
or a :class:`traceback.TracebackException`.

A consumer that retains the exception can set ``error.details = None`` to
remove its captures and cached graph. Other details objects and traceback
frames can still retain operands. Exception serialization omits capture data
and the cached graph. It preserves ordinary arguments and user attributes.
