"""Private formatter for the native assertion prototype.

The compiler emits a plan with one record per capture. Each optional value is
an empty tuple for a skipped expression, or a one-item tuple for an evaluated
expression. Raw values are references, not snapshots of mutable object state.
"""

import difflib
import reprlib


_repr = reprlib.Repr(maxstring=240, maxother=240, maxlist=12, maxtuple=12,
                    maxdict=12, maxset=12, maxlevel=4)


def _safe_repr(value):
    try:
        return _repr.repr(value)
    except Exception:
        return '<repr failed>'


def _plain(value, depth=0):
    """Permit diffs only when equality cannot invoke user-defined methods."""
    if type(value) in (str, bytes, int, float, bool, type(None)):
        return True
    if depth >= 4:
        return False
    if type(value) in (tuple, list, set, frozenset):
        return len(value) <= 100 and all(_plain(v, depth + 1) for v in value)
    if type(value) is dict:
        return len(value) <= 100 and all(
            _plain(k, depth + 1) and _plain(v, depth + 1)
            for k, v in value.items())
    return False


def _difference(left, right, op):
    if op != '==' or not (_plain(left) and _plain(right)):
        return []
    if type(left) is str and type(right) is str:
        # Bound the input to the diff algorithm as well as its output.
        if len(left) > 4000 or len(right) > 4000:
            return ['    string diff omitted: input exceeds 4000 characters\n']
        lines = difflib.ndiff(right.splitlines(), left.splitlines())
        return ['    ' + line + '\n' for _, line in zip(range(40), lines)]
    if type(left) in (list, tuple) and type(right) is type(left):
        for index, (a, b) in enumerate(zip(left, right)):
            if a != b:
                return [f'    At index {index} diff: '
                        f'{_safe_repr(a)} != {_safe_repr(b)}\n']
        if len(left) != len(right):
            return [f'    sequence lengths: {len(left)} != {len(right)}\n']
    if type(left) is dict and type(right) is dict:
        lines = []
        for key in left:
            if key not in right:
                lines.append(f'    only in left: {_safe_repr(key)}\n')
            elif left[key] != right[key]:
                lines.append(f'    key {_safe_repr(key)}: '
                             f'{_safe_repr(left[key])} != {_safe_repr(right[key])}\n')
            if len(lines) == 12:
                break
        for key in right:
            if len(lines) == 12:
                break
            if key not in left:
                lines.append(f'    only in right: {_safe_repr(key)}\n')
        return lines
    if type(left) in (set, frozenset) and type(right) is type(left):
        return [f'    only in left: {_safe_repr(left - right)}\n',
                f'    only in right: {_safe_repr(right - left)}\n']
    return []


class _Explanation:
    def __init__(self, details, compare):
        self.representations = {}
        self.comparisons = {}
        self.differences = []
        for comparison in details.comparisons:
            if comparison.evaluated:
                override = None
                if compare is not None:
                    result = compare(comparison.operator, comparison.left.value,
                                     comparison.right.value)
                    if result is not None:
                        if not isinstance(result, str):
                            raise TypeError('comparison formatter must return str or None')
                        override = result.splitlines()
                # A chain stops at its first false link.
                self.comparisons[comparison.expression] = comparison, override

    def represent(self, value):
        key = id(value)
        if key not in self.representations:
            self.representations[key] = _safe_repr(value)
        return self.representations[key]

    def comparison(self, node):
        chosen, override = self.comparisons[node]
        if override:
            self.differences.extend('    ' + line + '\n' for line in override[1:])
            return override[0], []
        left, left_bindings = self.render(chosen.left)
        right, right_bindings = self.render(chosen.right)
        if override is None and (chosen.truth is False or chosen.value is False):
            self.differences.extend(_difference(
                chosen.left.value, chosen.right.value, chosen.operator))
        return f'{left} {chosen.operator} {right}', left_bindings + right_bindings

    def compose(self, node):
        # Parse only the compiler's normalized text. Source offsets from that
        # parse let us substitute operands without changing literals or names.
        import ast
        tree = ast.parse(node.text, mode='eval').body
        if node.kind == 'binary':
            operands = [tree.left, tree.right]
        elif node.kind == 'unary':
            operands = [tree.operand]
        elif node.kind == 'attribute':
            operands = [tree.value]
        elif node.kind == 'subscript':
            operands = [tree.value, tree.slice]
        else:
            operands = [tree.func]
            operands.extend(arg.value if isinstance(arg, ast.Starred) else arg
                            for arg in tree.args)
            operands.extend(keyword.value for keyword in tree.keywords)

        source = node.text.encode('utf-8')
        offsets = [0]
        for line in source.splitlines(keepends=True):
            offsets.append(offsets[-1] + len(line))
        replacements = []
        bindings = []
        for index, (operand, child) in enumerate(
                zip(operands, node.children, strict=True)):
            if isinstance(operand, ast.Slice):
                # Constant-folded bounds need not have their own graph nodes.
                value = child.value
                bounds = [value.start, value.stop]
                if value.step is not None:
                    bounds.append(value.step)
                text = ':'.join(self.represent(bound) if bound is not None else ''
                                for bound in bounds)
                nested = []
            elif node.kind == 'call' and index == 0:
                if child.kind == 'attribute':
                    text, nested = self.compose(child)
                else:
                    text, nested = child.text, []
            else:
                text, nested = self.render(child)
            bindings.extend(nested)
            start = offsets[operand.lineno - 1] + operand.col_offset
            end = offsets[operand.end_lineno - 1] + operand.end_col_offset
            replacements.append((start, end, text.encode('utf-8')))
        for start, end, text in sorted(replacements, reverse=True):
            source = source[:start] + text + source[end:]
        return source.decode('utf-8'), bindings

    def render(self, node):
        if not node.evaluated:
            return node.text, []
        if node.kind == 'compare':
            return self.comparison(node)
        if node.kind in ('and', 'or'):
            parts, bindings = [], []
            for child in node.children:
                if not child.evaluated:
                    break
                text, nested = self.render(child)
                parts.append(text)
                bindings.extend(nested)
                if child.truth is (node.kind == 'or'):
                    break
            return '(' + f' {node.kind} '.join(parts) + ')', bindings
        if node.kind == 'conditional':
            condition, body, otherwise = node.children
            return self.render(body if condition.truth is True else otherwise)
        if node.kind == 'not':
            text, bindings = self.render(node.children[0])
            return f'not ({text})', bindings
        if node.kind in ('binary', 'unary', 'call', 'attribute', 'subscript'):
            text, bindings = self.compose(node)
            if node.kind == 'binary':
                return f'({text})', bindings
            if node.kind == 'unary':
                return text, bindings
            value = self.represent(node.value)
            return value, [(f'{value} = {text}', bindings)]
        return self.represent(node.value), []


def _format_bindings(bindings, depth=0):
    for index, (text, children) in enumerate(bindings):
        label = 'where' if index == 0 else 'and'
        yield '    ' + '  ' * depth + f'+ {label} {text}\n'
        yield from _format_bindings(children, depth + 1)


def format_details(details, *, compare=None):
    """Return a compact explanation from captured values, without evaluation."""
    explanation = _Explanation(details, compare)
    text, bindings = explanation.render(details.expression)
    lines = [f'  assert {text}\n', *_format_bindings(bindings), *explanation.differences]
    return ''.join(line[2:] for line in lines).rstrip('\n')
