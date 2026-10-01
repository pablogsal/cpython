"""Compare sampling failures on a single-threaded target, using Python 3.15.

Run: python diagnose.py [--target target.py] [--seconds 3] [--no-cache]
Windows x64 also tests GetThreadContext after pause_threads().
"""

import argparse
from collections import Counter
import contextlib
import ctypes
import os
import platform
import runpy
import subprocess
import sys
import threading
import time


def leaf(x):
    return x + 1


def calls():
    x = 0
    while True:
        x = leaf(leaf(leaf(x)))


def worker(target):
    print(threading.get_native_id(), flush=True)
    sys.stdin.readline()
    with open(os.devnull, "w") as output, contextlib.redirect_stdout(output):
        if target:
            sys.argv = [target]
            sys.path.insert(0, os.path.dirname(os.path.abspath(target)))
            runpy.run_path(target, run_name="__main__")
        else:
            calls()


@contextlib.contextmanager
def windows_context(tid):
    # Only the main thread of the explicitly single-threaded target is checked.
    # CONTEXT layout: https://learn.microsoft.com/windows/win32/api/winnt/ns-winnt-context
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenThread.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    kernel32.OpenThread.restype = wintypes.HANDLE
    kernel32.GetThreadContext.argtypes = (wintypes.HANDLE, ctypes.c_void_p)
    kernel32.GetThreadContext.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = (wintypes.HANDLE,)
    kernel32.CloseHandle.restype = wintypes.BOOL
    handle = kernel32.OpenThread(0x0008, False, tid)  # THREAD_GET_CONTEXT
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        # AMD64 CONTEXT is 1232 bytes, aligned to 16 bytes. ContextFlags follows
        # six 64-bit home slots. Only control registers are requested.
        storage = ctypes.create_string_buffer(1232 + 15)
        address = (ctypes.addressof(storage) + 15) & ~15
        ctypes.c_uint32.from_address(address + 48).value = 0x00100001

        def synchronize():
            if not kernel32.GetThreadContext(handle, address):
                raise ctypes.WinError(ctypes.get_last_error())

        yield synchronize
    finally:
        kernel32.CloseHandle(handle)


def error_chain(exc):
    parts = []
    while exc is not None:
        parts.append(f"{type(exc).__name__}: {exc}")
        exc = exc.__cause__
    return " <- ".join(parts)


def measure(args, mode):
    from _remote_debugging import RemoteUnwinder

    command = [sys.executable, os.path.abspath(__file__), "--worker"]
    if args.target:
        command += ["--target", args.target]
    errors = Counter()
    retries = Counter()
    count = 0
    with subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          text=True) as child:
        try:
            tid = int(child.stdout.readline())
            unwinder = RemoteUnwinder(child.pid, native=True, opcodes=True, gc=True,
                                      cache_frames=not args.no_cache, debug=True)
            manager = windows_context(tid) if mode == "context" else contextlib.nullcontext()
            with manager as synchronize:
                child.stdin.write("go\n")
                child.stdin.flush()
                time.sleep(0.1)
                start = deadline = time.perf_counter()
                while time.perf_counter() - start < args.seconds and child.poll() is None:
                    paused = False
                    try:
                        if mode != "live":
                            unwinder.pause_threads()
                            paused = True
                        if mode == "delay":
                            time.sleep(0.001)
                        elif mode == "context":
                            synchronize()
                        try:
                            unwinder.get_stack_trace()
                        except (RuntimeError, OSError, UnicodeError, MemoryError) as exc:
                            errors[error_chain(exc)] += 1
                            # Check a few failures again without resuming the target.
                            if paused and sum(retries.values()) < 10:
                                time.sleep(0.001)
                                try:
                                    unwinder.get_stack_trace()
                                except (RuntimeError, OSError, UnicodeError, MemoryError) as again:
                                    retries[error_chain(again)] += 1
                                else:
                                    retries["retry succeeded while still paused"] += 1
                    finally:
                        if paused:
                            unwinder.resume_threads()
                    count += 1
                    deadline += 0.001
                    remaining = deadline - time.perf_counter()
                    if remaining > 0:
                        time.sleep(remaining)
                elapsed = time.perf_counter() - start
        finally:
            if child.poll() is None:
                child.terminate()
            child.wait()
    failed = sum(errors.values())
    rate = 100 * failed / count if count else 0
    print(f"{mode}: {failed}/{count} failed ({rate:.2f}%), {elapsed:.3f}s", flush=True)
    for message, frequency in errors.most_common(5):
        print(f"  {frequency}: {message}", flush=True)
    for message, frequency in retries.most_common(5):
        print(f"  same-pause retry {frequency}: {message}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", help="Optional single-threaded Python script")
    parser.add_argument("--seconds", type=float, default=3)
    parser.add_argument("--no-cache", action="store_true")
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.worker:
        worker(args.target)
        return
    print(sys.version)
    print(platform.platform(), platform.processor(), f"cache={not args.no_cache}", flush=True)
    modes = ["live", "pause", "delay"]
    if sys.platform == "win32" and platform.machine().lower() in ("amd64", "x86_64") and ctypes.sizeof(ctypes.c_void_p) == 8:
        modes.append("context")
    for mode in modes:
        measure(args, mode)


if __name__ == "__main__":
    main()
