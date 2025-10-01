from abc import ABC, abstractmethod

# Thread status flags
try:
    from _remote_debugging import (
        THREAD_STATUS_HAS_GIL,
        THREAD_STATUS_ON_CPU,
        THREAD_STATUS_UNKNOWN,
        THREAD_STATE_RUNNING,
        THREAD_STATE_IDLE,
        THREAD_STATE_GIL_WAIT,
    )
except ImportError:
    # Fallback for tests or when module is not available
    THREAD_STATUS_HAS_GIL = (1 << 0)
    THREAD_STATUS_ON_CPU = (1 << 1)
    THREAD_STATUS_UNKNOWN = (1 << 2)
    THREAD_STATE_RUNNING = 0
    THREAD_STATE_IDLE = 1
    THREAD_STATE_GIL_WAIT = 2

class Collector(ABC):
    def __init__(self):
        self.native_symbol_map = {}

    @abstractmethod
    def collect(self, stack_frames):
        """Collect profiling data from stack frames."""

    @abstractmethod
    def export(self, filename):
        """Export collected data to a file."""

    def set_native_symbol_map(self, symbol_map):
        """Set the symbolization map for native IPs."""
        self.native_symbol_map = symbol_map

    def _merge_native_and_python_frames(self, python_frames, native_ips):
        """Merge native and Python frames based on entry frame markers.

        For each Python frame with is_entry_frame=True, we insert the corresponding
        native frames that called into Python. The native frames are in reverse order
        (most recent first), and we need to match them to entry frames.

        Args:
            python_frames: List of Python FrameInfo objects with is_entry_frame attribute
            native_ips: List of native instruction pointers (integers) for this thread

        Returns:
            List of frame tuples where each is either:
            - ('python', frame_info) for Python frames
            - ('native', func_name, offset, module) for native frames
        """
        if not native_ips or not self.native_symbol_map:
            # No native frames, just return Python frames
            return [('python', frame) for frame in python_frames]

        merged = []
        native_idx = 0

        for frame in python_frames:
            if frame.is_entry_frame and native_idx < len(native_ips):
                # This is an entry frame - insert native frames before it
                # Native frames between this entry and the next entry (or end)
                # We insert them in order (outermost to innermost)
                # Find the next entry frame to know how many native frames to include
                # For now, insert all remaining native frames before first entry frame
                # or distribute them between entry frames
                if native_idx == 0:
                    # Insert all native frames before the first entry frame
                    while native_idx < len(native_ips):
                        ip = native_ips[native_idx]
                        symbol = self.native_symbol_map.get(ip, (None, ip, None))
                        merged.append(('native', symbol[0], symbol[1], symbol[2]))
                        native_idx += 1

            # Add the Python frame
            merged.append(('python', frame))

        return merged

    def _iter_all_frames(self, stack_frames, skip_idle=False):
        """Iterate over all frame stacks from all interpreters and threads."""
        for interpreter_info in stack_frames:
            for thread_info in interpreter_info.threads:
                # skip_idle now means: skip if thread is not actively running
                # A thread is "active" if it has the GIL OR is on CPU
                if skip_idle:
                    status_flags = thread_info.status
                    has_gil = bool(status_flags & THREAD_STATUS_HAS_GIL)
                    on_cpu = bool(status_flags & THREAD_STATUS_ON_CPU)
                    if not (has_gil or on_cpu):
                        continue
                frames = thread_info.frame_info
                if frames:
                    yield frames, thread_info.thread_id
