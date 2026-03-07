import unittest

from _pyrepl.rendering import RenderFrame, diff_frames


class TestRendering(unittest.TestCase):
    def test_viewport_offset_follows_cursor(self):
        frame = RenderFrame(
            lines=["0", "1", "2", "3", "4"],
            cursor_xy=(0, 4),
            width=80,
            height=2,
        )
        self.assertEqual(frame.viewport_offset(previous_offset=0), 3)

    def test_insert_diff_uses_insert_cells_when_dimensions_stable(self):
        previous = RenderFrame(
            lines=["abcd"],
            cursor_xy=(4, 0),
            width=80,
            height=24,
        )
        current = RenderFrame(
            lines=["abXcd"],
            cursor_xy=(5, 0),
            width=80,
            height=24,
        )

        diff, _ = diff_frames(previous, current, previous_offset=0)
        self.assertEqual(len(diff.row_updates), 1)
        _, row = diff.row_updates[0]
        self.assertEqual(row.insert_cells, 1)
        self.assertFalse(row.erase_to_eol)

    def test_width_change_forces_full_row_rewrite_with_clear(self):
        previous = RenderFrame(
            lines=["abcdef"],
            cursor_xy=(6, 0),
            width=20,
            height=5,
        )
        current = RenderFrame(
            lines=["abcdef"],
            cursor_xy=(6, 0),
            width=80,
            height=5,
        )

        diff, _ = diff_frames(previous, current, previous_offset=0)
        self.assertEqual(len(diff.row_updates), current.height)
        _, row = diff.row_updates[0]
        self.assertEqual(row.start_x, 0)
        self.assertTrue(row.erase_to_eol)
        self.assertEqual(row.insert_cells, 0)

    def test_clear_rows_when_current_visible_rows_shrink(self):
        previous = RenderFrame(
            lines=["line0", "line1", "line2"],
            cursor_xy=(0, 2),
            width=80,
            height=4,
        )
        current = RenderFrame(
            lines=["line0"],
            cursor_xy=(0, 0),
            width=80,
            height=4,
        )

        diff, offset = diff_frames(previous, current, previous_offset=0)
        self.assertEqual(offset, 0)
        self.assertEqual(diff.clear_rows_from, 1)


if __name__ == "__main__":
    unittest.main()
