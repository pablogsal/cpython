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

# Global cache for symbolized native frames: IP -> _NativeFrameInfo
_symbolization_cache = {}

class _NativeFrameInfo:
    """Simple container for native frame information.

    Provides .funcname attribute for compatibility with the merge logic.
    Also provides .filename for stack collapse format output.
    Supports indexing like FrameInfo structseq: [0]=filename, [1]=location, [2]=funcname
    """
    __slots__ = ('funcname', 'offset', 'module', 'filename', 'location')

    def __init__(self, funcname, offset, module):
        self.funcname = funcname
        self.offset = offset
        self.module = module
        self.location = None  # Native frames don't have line numbers
        # Use actual module path from dladdr as filename
        if module:
            self.filename = module
        else:
            self.filename = "<unknown>"

    def __getitem__(self, idx):
        # Match FrameInfo structseq layout: (filename, location, funcname, opcode, is_entry)
        if idx == 0:
            return self.filename
        elif idx == 1:
            return self.location
        elif idx == 2:
            return self.funcname
        elif idx == 3:
            return None  # opcode
        elif idx == 4:
            return False  # is_entry
        raise IndexError(f"index {idx} out of range")

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

    Strategy:
    1. Walk through native frames from top (innermost) to bottom (outermost)
    2. When we encounter _PyEval_EvalFrameDefault, substitute with Python frames
    3. Keep other native frames (libc, C extensions, etc.)
    4. Filter out internal Python implementation frames (vectorcall, etc.)

    Args:
        thread_info: A ThreadInfo namedtuple with fields:
            - thread_id: The thread ID
            - status: Thread status flags
            - frame_info: List of Python FrameInfo objects
            - native_frames: List of native instruction pointers (integers)
        unwinder: RemoteUnwinder object with symbolize_native_backtrace() method

    Returns:
        List of FrameInfo objects representing the merged hybrid stack.
    """
    python_frames = list(thread_info.frame_info)
    native_frames = thread_info.native_frames
    tid = thread_info.thread_id

    # If no native frames, just return Python frames as-is
    if not native_frames:
        return python_frames

    # Symbolize native frames using cache
    # native_frames is now a list of integers (instruction pointers)
    symbolized_native = []
    ips_to_symbolize = []
    ips_to_symbolize_indices = []

    for idx, ip in enumerate(native_frames):
        if ip in _symbolization_cache:
            symbolized_native.append(_symbolization_cache[ip])
        else:
            symbolized_native.append(None)  # Placeholder
            ips_to_symbolize.append(ip)
            ips_to_symbolize_indices.append(idx)

    if ips_to_symbolize:
        # symbolize_native_backtrace returns list of (funcname, offset, module) tuples
        newly_symbolized = unwinder.symbolize_native_backtrace(ips_to_symbolize, tid)
        for i, symbol_tuple in enumerate(newly_symbolized):
            idx = ips_to_symbolize_indices[i]
            ip = ips_to_symbolize[i]
            funcname, offset, module = symbol_tuple
            # Create a simple object that has .funcname attribute for compatibility
            frame_info = _NativeFrameInfo(funcname or f"<0x{ip:x}>", offset, module)
            _symbolization_cache[ip] = frame_info
            symbolized_native[idx] = frame_info

    # Simple merge: substitute _PyEval_EvalFrameDefault with Python frames
    # Walk native stack from top to bottom
    hybrid_stack = []
    python_frames_added = False

    for native_frame in symbolized_native:
        funcname = native_frame.funcname

        if _is_eval_frame(funcname):
            # First time we see PyEval, insert all Python frames
            if not python_frames_added:
                hybrid_stack.extend(python_frames)
                python_frames_added = True
            # Skip the PyEval frame itself (we're showing Python frames instead)
            continue

        elif _should_ignore_frame(funcname):
            # Skip internal Python implementation frames
            continue
        else:
            # Keep this native frame (libc, C extensions, etc.)
            hybrid_stack.append(native_frame)

    # If we never saw a PyEval frame (shouldn't happen), add Python frames at the end
    if not python_frames_added and python_frames:
        hybrid_stack.extend(python_frames)

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
