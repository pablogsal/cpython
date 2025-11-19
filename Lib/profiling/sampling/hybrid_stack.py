"""
Hybrid Stack Merging for Native Profiling

This module provides utilities to merge Python and native stacks into a unified
hybrid stack trace, similar to how Austin handles native unwinding.

The strategy (based on Austin's implementation):
1. Walk through the native stack frames
2. When we encounter an EVAL frame (_PyEval_EvalFrameDefault), substitute it with
   the actual Python frames (from the current entry frame to the next entry frame)
3. Keep OTHER native frames as-is (like libc, custom C extensions, etc.)
4. IGNORE internal Python frames (like _PyEval_EvalFrame, vectorcall wrappers, etc.)

This gives us a merged stack showing both Python and C code execution.
"""

# Global cache for symbolized native frames: IP -> FrameInfo
_symbolization_cache = {}

# Symbols that indicate we're in the Python eval loop
EVAL_FRAME_SYMBOLS = {
    "_PyEval_EvalFrameDefault",
    "PyEval_EvalFrameEx",
}

# Symbols to ignore (internal Python implementation details)
IGNORE_SYMBOLS = {
    "PyEval_",
    "_PyEval_",
    "_Py",
    "PyObject_Call",
    "call_function",
    "vectorcall",
    "cfunction_vectorcall",
}

def _should_ignore_frame(funcname):
    """Check if a native frame should be ignored (internal Python details)."""
    if any(funcname.startswith(prefix) for prefix in IGNORE_SYMBOLS):
        # But don't ignore EVAL frames - we substitute those
        if any(eval_sym in funcname for eval_sym in EVAL_FRAME_SYMBOLS):
            return False
        return True
    if "vectorcall" in funcname.lower():
        return True
    return False

def _is_eval_frame(funcname):
    """Check if this is a PyEval frame that should be substituted."""
    return any(eval_sym in funcname for eval_sym in EVAL_FRAME_SYMBOLS)

def merge_stacks(thread_info, unwinder):
    """
    Merge Python and native frames into a hybrid stack.

    This function takes a ThreadInfo object (which contains both Python frames
    and native frames) and merges them into a single unified stack trace.

    The merge is done at "entry frames" - frames that mark boundaries where
    Python code calls native code. Between two entry frames, we substitute
    the PyEval_EvalFrameDefault frames with the actual native frames.

    Args:
        thread_info: A ThreadInfo namedtuple with fields:
            - thread_id: The thread ID
            - status: Thread status flags
            - frame_info: List of Python FrameInfo objects
            - native_frames: List of native frame tuples (pc, func_name, offset)
        unwinder: RemoteUnwinder object with symbolize_native_frames() method

    Returns:
        List of FrameInfo objects representing the merged hybrid stack.
        Python frames have is_entry=True/False, native frames have is_entry=False.

    Example:
        Before merging (Python frames only):
            Frame 0: my_function() [is_entry=False]
            Frame 1: wrapper() [is_entry=True]     <- Entry frame
            Frame 2: main() [is_entry=True]        <- Entry frame

        After merging (hybrid stack):
            Frame 0: my_function() [is_entry=False]
            Frame 1: wrapper() [is_entry=True]     <- Entry frame
            Frame 2: PyEval_EvalFrameDefault+0x123 [is_entry=False] <- Native
            Frame 3: _PyEval_Vector+0x45 [is_entry=False]           <- Native
            Frame 4: main() [is_entry=True]        <- Entry frame
            Frame 5: Py_Main+0x67 [is_entry=False]                   <- Native
    """
    python_frames = list(thread_info.frame_info)
    native_frames = thread_info.native_frames

    # If no native frames, just return Python frames as-is
    if not native_frames:
        return python_frames

    # Symbolize native frames using cache
    # First, separate cached vs uncached IPs
    symbolized_native = []
    frames_to_symbolize = []
    frames_to_symbolize_indices = []

    for idx, (pc, offset) in enumerate(native_frames):
        if pc in _symbolization_cache:
            # Use cached symbolization
            symbolized_native.append(_symbolization_cache[pc])
        else:
            # Need to symbolize this one
            symbolized_native.append(None)  # Placeholder
            frames_to_symbolize.append((pc, offset))
            frames_to_symbolize_indices.append(idx)

    # Symbolize the uncached frames
    if frames_to_symbolize:
        newly_symbolized = unwinder.symbolize_native_frames(frames_to_symbolize)

        # Update cache and fill in placeholders
        for i, frame_info in enumerate(newly_symbolized):
            idx = frames_to_symbolize_indices[i]
            pc = frames_to_symbolize[i][0]
            _symbolization_cache[pc] = frame_info
            symbolized_native[idx] = frame_info

    # Austin's algorithm: Walk through native frames, substituting EVAL frames with Python frames
    hybrid_stack = []
    python_idx = 0  # Current position in Python frames

    for native_frame in symbolized_native:
        funcname = native_frame.funcname

        if _is_eval_frame(funcname):
            # Substitute this EVAL frame with the actual Python frames
            # Add the current entry frame (if we have one)
            if python_idx < len(python_frames):
                current_py_frame = python_frames[python_idx]
                if current_py_frame.is_entry:
                    hybrid_stack.append(current_py_frame)
                    python_idx += 1

                    # Add all non-entry frames until the next entry frame
                    while python_idx < len(python_frames) and not python_frames[python_idx].is_entry:
                        hybrid_stack.append(python_frames[python_idx])
                        python_idx += 1

        elif _should_ignore_frame(funcname):
            # Skip internal Python implementation frames
            continue
        else:
            # Keep this native frame (it's from C extensions, libc, etc.)
            hybrid_stack.append(native_frame)

    return hybrid_stack


def is_pyeval_frame(frame_info):
    """
    Check if a FrameInfo represents a PyEval_EvalFrameDefault or similar frame.

    These frames are internal Python VM frames that are redundant when we have
    the corresponding Python frames.
    """
    func_name = frame_info.funcname
    return 'PyEval_EvalFrameDefault' in func_name or \
           '_PyEval_EvalFrameEx' in func_name or \
           'PyEval_EvalFrame' == func_name


def filter_redundant_frames(frames):
    """
    Remove redundant PyEval frames from a frame list.

    When we have both Python frames and native frames, the PyEval frames
    are redundant since we already have the Python source-level information.
    """
    return [f for f in frames if not is_pyeval_frame(f)]
