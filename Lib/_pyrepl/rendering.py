from __future__ import annotations

from dataclasses import dataclass
import re

from .utils import str_width

# Keep this local so the diff engine doesn't depend on console internals.
_ANSI_ESCAPE_SEQUENCE = re.compile(r"\x1b\[[ -@]*[A-~]")


@dataclass(frozen=True)
class Cell:
    char: str
    width: int
    style: str = ""
    prefix: str = ""


@dataclass(frozen=True)
class RenderLine:
    cells: tuple[Cell, ...]
    width: int


@dataclass(frozen=True)
class RowUpdate:
    start_x: int
    cells: tuple[Cell, ...]
    erase_to_eol: bool
    insert_cells: int = 0


@dataclass(frozen=True)
class DiffUpdate:
    row_updates: list[tuple[int, RowUpdate]]
    clear_rows_from: int | None = None


@dataclass
class RenderFrame:
    lines: list[str]
    cursor_xy: tuple[int, int]
    width: int
    height: int

    def viewport_offset(self, previous_offset: int) -> int:
        _, cy = self.cursor_xy
        offset = previous_offset
        if cy < offset:
            offset = cy
        elif cy >= offset + self.height:
            offset = cy - self.height + 1
        elif offset > 0 and len(self.lines) < offset + self.height:
            offset = max(len(self.lines) - self.height, 0)
        return offset


def parse_line(line: str) -> RenderLine:
    """Parse a terminal line into display cells with normalized style state."""
    cells: list[Cell] = []
    style = ""
    prefix = ""
    i = 0
    n = len(line)

    while i < n:
        if line[i] == "\x1b":
            match = _ANSI_ESCAPE_SEQUENCE.match(line, i)
            if match:
                seq = match.group(0)
                # Track only SGR state; other escape sequences are injected.
                if seq.endswith("m"):
                    if seq == "\x1b[0m":
                        style = ""
                    else:
                        style += seq
                else:
                    prefix += seq
                i = match.end()
                continue

        char = line[i]
        width = 2 if char == "\x1a" else str_width(char)
        cells.append(Cell(char=char, width=width, style=style, prefix=prefix))
        prefix = ""
        i += 1

    return RenderLine(tuple(cells), sum(cell.width for cell in cells))


def _visible_lines(lines: list[str], offset: int, height: int, *, pad: bool) -> list[str]:
    visible = lines[offset : offset + height]
    if pad and len(visible) < height:
        visible = visible + [""] * (height - len(visible))
    return visible


def _matching_prefix_len(old: tuple[Cell, ...], new: tuple[Cell, ...]) -> int:
    i = 0
    limit = min(len(old), len(new))
    while i < limit and old[i] == new[i]:
        i += 1
    return i


def _matching_suffix_len(old: tuple[Cell, ...], new: tuple[Cell, ...], prefix: int) -> int:
    i = len(old) - 1
    j = len(new) - 1
    matched = 0
    while i >= prefix and j >= prefix and old[i] == new[j]:
        matched += 1
        i -= 1
        j -= 1
    return matched


def _row_update(old: RenderLine, new: RenderLine, *, allow_insert: bool) -> RowUpdate | None:
    if old == new:
        return None

    prefix = _matching_prefix_len(old.cells, new.cells)
    suffix = _matching_suffix_len(old.cells, new.cells, prefix)
    old_mid = old.cells[prefix : len(old.cells) - suffix]
    new_mid = new.cells[prefix : len(new.cells) - suffix]

    # Update only the changed middle segment when old/new display widths match.
    if old.width == new.width and suffix:
        changed = new_mid
        if changed:
            return RowUpdate(
                start_x=sum(cell.width for cell in new.cells[:prefix]),
                cells=changed,
                erase_to_eol=False,
            )

    # Pure insertion in the middle: emit only inserted cells and ask backend
    # for column insert support if available.
    if allow_insert and not old_mid and new_mid and suffix:
        return RowUpdate(
            start_x=sum(cell.width for cell in new.cells[:prefix]),
            cells=new_mid,
            erase_to_eol=False,
            insert_cells=sum(cell.width for cell in new_mid),
        )

    # Fallback: rewrite from first changed cell; erase trailing old content.
    changed = new.cells[prefix:]
    return RowUpdate(
        start_x=sum(cell.width for cell in new.cells[:prefix]),
        cells=changed,
        erase_to_eol=old.width > new.width,
    )


def diff_frames(
    previous: RenderFrame | None,
    current: RenderFrame,
    previous_offset: int,
) -> tuple[DiffUpdate, int]:
    offset = current.viewport_offset(previous_offset)
    visible = _visible_lines(current.lines, offset, current.height, pad=previous is not None)
    absolute_rows = range(offset, offset + current.height)

    if previous is None:
        updates: list[tuple[int, RowUpdate]] = []
        for y, line in zip(range(offset, offset + len(visible)), visible):
            parsed = parse_line(line)
            updates.append((y, RowUpdate(start_x=0, cells=parsed.cells, erase_to_eol=False)))
        return DiffUpdate(updates), offset

    prev_visible = _visible_lines(previous.lines, previous_offset, previous.height, pad=True)
    if previous.width != current.width:
        # Width changes expose/hide columns. A minimal patch can leave stale
        # cells in newly exposed columns, so rewrite visible rows safely.
        updates: list[tuple[int, RowUpdate]] = []
        for y, line in zip(range(offset, offset + len(visible)), visible):
            parsed = parse_line(line)
            updates.append((y, RowUpdate(start_x=0, cells=parsed.cells, erase_to_eol=True)))
        return DiffUpdate(updates), offset

    allow_insert = (
        previous.height == current.height
    )
    updates = []
    for y, old_line, new_line in zip(absolute_rows, prev_visible, visible):
        update = _row_update(
            parse_line(old_line),
            parse_line(new_line),
            allow_insert=allow_insert,
        )
        if update is not None:
            updates.append((y, update))

    clear_rows_from = None
    current_visible_rows = min(max(len(current.lines) - offset, 0), current.height)
    previous_visible_rows = min(
        max(len(previous.lines) - previous_offset, 0),
        previous.height,
    )
    if current_visible_rows < previous_visible_rows:
        clear_rows_from = offset + current_visible_rows

    return DiffUpdate(updates, clear_rows_from=clear_rows_from), offset
