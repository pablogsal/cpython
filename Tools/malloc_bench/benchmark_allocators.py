#!/usr/bin/env python3
"""
Benchmark: PYTHONMALLOC=pymalloc vs PYTHONMALLOC=malloc

Measures CPU time, wall time, and provides deep analysis of WHY
the two allocators behave differently.

Usage:
    python Tools/malloc_bench/benchmark_allocators.py

Or run under a specific allocator:
    PYTHONMALLOC=pymalloc python Tools/malloc_bench/benchmark_allocators.py
    PYTHONMALLOC=malloc  python Tools/malloc_bench/benchmark_allocators.py

The script auto-detects the current allocator and can also launch
sub-processes to compare both side-by-side.
"""

import sys
import os
import time
import resource
import subprocess
import json
import statistics
import gc

# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------

def get_allocator_name():
    """Return the active allocator name via sys internals."""
    return os.environ.get("PYTHONMALLOC", "default (pymalloc)")


def cpu_time():
    """Return user+system CPU time for this process."""
    r = resource.getrusage(resource.RUSAGE_SELF)
    return r.ru_utime + r.ru_stime


def bench(fn, *, label, warmup=3, iterations=7):
    """Run *fn* and collect wall / CPU / RSS measurements.

    GC is disabled during measurement to avoid noise from collection
    cycles that would confound allocator timing.
    """
    # warmup with GC on to reach steady state
    for _ in range(warmup):
        fn()

    gc.collect()
    gc.disable()

    walls, cpus = [], []
    for _ in range(iterations):
        c0 = cpu_time()
        w0 = time.perf_counter()
        fn()
        w1 = time.perf_counter()
        c1 = cpu_time()
        walls.append(w1 - w0)
        cpus.append(c1 - c0)

    gc.enable()
    gc.collect()

    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss  # KiB on Linux
    return {
        "label": label,
        "wall_mean": statistics.mean(walls),
        "wall_stdev": statistics.stdev(walls) if len(walls) > 1 else 0,
        "cpu_mean": statistics.mean(cpus),
        "cpu_stdev": statistics.stdev(cpus) if len(cpus) > 1 else 0,
        "rss_kib": rss,
        "iterations": iterations,
    }


# ---------------------------------------------------------------------------
# Micro-benchmarks — each exercises a different allocator pressure pattern
# ---------------------------------------------------------------------------

# ---- Category 1: Pure Python object allocation (OBJ domain) ----

def bench_tuple_alloc():
    """Allocate 500K 2-tuples via comprehension.

    2-tuples are 40 bytes on 64-bit → size class 2 (48 B) in pymalloc.
    This is the purest OBJ-domain allocator benchmark: no buffer
    allocation, just tp_alloc → PyObject_Malloc.
    """
    data = [(i, i) for i in range(500_000)]
    del data


def bench_list_creation():
    """Allocate 500K small lists (3 elements each).

    Each list = PyListObject header (40B) + ob_item array (3*8=24B).
    Both are <512B → pymalloc territory.
    """
    data = [[1, 2, 3] for _ in range(500_000)]
    del data


def bench_dict_creation():
    """Allocate 300K small dicts (3 key-value pairs).

    dict = PyDictObject (48B) + keys object + values table.
    All pieces <512B for small dicts.
    """
    data = [{"a": 1, "b": 2, "c": 3} for _ in range(300_000)]
    del data


def bench_class_instances():
    """Allocate 500K __slots__ instances.

    tp_alloc → PyObject_Malloc for objects with 2 slots (~48B).
    Pure OBJ domain allocation, no extra buffers.
    """
    class Point:
        __slots__ = ("x", "y")
        def __init__(self, x, y):
            self.x = x
            self.y = y

    objs = [Point(i, i + 1) for i in range(500_000)]
    del objs


def bench_empty_bytes():
    """Allocate 500K small bytes objects (32 bytes payload).

    bytes objects are immutable with inline storage for small sizes.
    Header + data is ~65B → fits in a single pymalloc block.
    """
    data = [bytes(32) for _ in range(500_000)]
    del data


# ---- Category 2: Allocation + deallocation churn ----

def bench_rapid_churn():
    """Create and immediately destroy objects — tests alloc+free cycle.

    This tests the allocator's ability to recycle recently-freed blocks.
    pymalloc's free-list should give O(1) reuse; malloc's tcache or
    fastbins serve a similar role.
    """
    for _ in range(1_000_000):
        obj = [1, 2, 3]
        del obj


def bench_dict_churn():
    """Create and destroy dicts with nested structure.

    Each iteration creates ~3 objects (outer dict, inner dict, and
    their key tables), then frees them all.
    """
    for _ in range(200_000):
        d = {"name": "benchmark", "value": 42, "nested": {"a": 1, "b": 2}}
        del d


# ---- Category 3: Fragmentation patterns ----

def bench_fragmentation():
    """Interleaved alloc/free — the classic fragmentation stress test.

    Phase 1: allocate 500K same-size objects
    Phase 2: free every other one (create holes)
    Phase 3: re-allocate into the holes

    pymalloc: same-size pools mean freed blocks are immediately
    reusable with O(1) free-list pop. No fragmentation.

    malloc: freed chunks may be coalesced or remain in bins;
    re-allocation must find appropriately-sized chunks.
    """
    N = 500_000
    objs = [None] * N
    for i in range(N):
        objs[i] = (i, i + 1)  # 40-byte 2-tuples
    for i in range(0, N, 2):
        objs[i] = None
    for i in range(0, N, 2):
        objs[i] = (i, i + 1)
    del objs


def bench_mixed_size_frag():
    """Alternating small and medium allocations, then selective free.

    This creates size-class interleaving that pymalloc handles cleanly
    (separate pools per class) but can fragment malloc's heap.
    """
    N = 300_000
    objs = [None] * N
    for i in range(N):
        if i % 2 == 0:
            objs[i] = bytes(16)   # tiny
        else:
            objs[i] = bytes(256)  # medium (still <512)
    # Free all the small ones
    for i in range(0, N, 2):
        objs[i] = None
    # Reallocate as medium
    for i in range(0, N, 2):
        objs[i] = bytes(256)
    del objs


# ---- Category 4: Burst allocation (syscall/arena pressure) ----

def bench_burst_small():
    """Burst-allocate 1M small objects, then bulk free.

    Forces pymalloc to allocate many new arenas (each 1 MiB via mmap).
    On free, entire arenas may be returned via munmap.

    With malloc, glibc extends the heap via brk() or uses mmap for
    larger backing chunks.
    """
    objs = [bytes(64) for _ in range(1_000_000)]
    del objs


def bench_burst_medium():
    """Burst-allocate objects near the pymalloc threshold.

    bytes(480) → ~513B total with header → right at the boundary.
    Tests the size-check overhead in pymalloc's fast path.
    """
    objs = [bytes(480) for _ in range(200_000)]
    del objs


# ---- Category 5: Control benchmarks ----

def bench_large_alloc_control():
    """Allocate >512B objects — BOTH allocators use raw malloc.

    This is the control benchmark. Since pymalloc delegates to raw
    malloc for requests >512B, both modes should show similar
    performance. Any difference indicates overhead in pymalloc's
    size-check fallthrough path.
    """
    objs = [bytes(2048) for _ in range(200_000)]
    del objs


def bench_computation_control():
    """Pure computation, no allocation — verifies equal baseline.

    If this benchmark shows a difference between allocators, something
    is wrong with the measurement methodology.
    """
    total = 0
    for i in range(2_000_000):
        total += i * i
    return total


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

ALL_BENCHMARKS = [
    # Pure object allocation
    (bench_tuple_alloc,       "tuple_alloc       (2-tuples × 500K)"),
    (bench_list_creation,     "list_creation     ([1,2,3] × 500K)"),
    (bench_dict_creation,     "dict_creation     (3-item × 300K)"),
    (bench_class_instances,   "class_instances   (__slots__ × 500K)"),
    (bench_empty_bytes,       "empty_bytes       (32B × 500K)"),
    # Churn
    (bench_rapid_churn,       "rapid_churn       (alloc+free × 1M)"),
    (bench_dict_churn,        "dict_churn        (nested dicts × 200K)"),
    # Fragmentation
    (bench_fragmentation,     "fragmentation     (interleaved × 500K)"),
    (bench_mixed_size_frag,   "mixed_size_frag   (16B+256B × 300K)"),
    # Burst
    (bench_burst_small,       "burst_small       (64B × 1M)"),
    (bench_burst_medium,      "burst_medium      (480B × 200K)"),
    # Controls
    (bench_large_alloc_control, "large_alloc_ctrl  (2KiB × 200K) [ctrl]"),
    (bench_computation_control, "computation_ctrl  (pure math) [ctrl]"),
]


def run_benchmarks():
    alloc = get_allocator_name()
    results = {"allocator": alloc, "benchmarks": [], "python": sys.version}
    print(f"\n{'=' * 70}", file=sys.stderr)
    print(f"  Allocator: {alloc}", file=sys.stderr)
    print(f"  Python:    {sys.version}", file=sys.stderr)
    print(f"  PID:       {os.getpid()}", file=sys.stderr)
    print(f"{'=' * 70}\n", file=sys.stderr)
    fmt = f"  {'Benchmark':<42} {'Wall (ms)':>10} {'CPU (ms)':>10} {'RSS (KiB)':>10}"
    print(fmt, file=sys.stderr)
    print(f"  {'-'*42} {'-'*10} {'-'*10} {'-'*10}", file=sys.stderr)

    for fn, label in ALL_BENCHMARKS:
        r = bench(fn, label=label)
        results["benchmarks"].append(r)
        print(
            f"  {label:<42} "
            f"{r['wall_mean']*1000:>9.1f}  "
            f"{r['cpu_mean']*1000:>9.1f}  "
            f"{r['rss_kib']:>10}",
            file=sys.stderr,
        )

    print(file=sys.stderr)
    return results


def run_comparison():
    """Launch two sub-processes (pymalloc vs malloc) and compare."""
    python = sys.executable
    script = os.path.abspath(__file__)

    def run_with(allocator):
        env = os.environ.copy()
        env["PYTHONMALLOC"] = allocator
        env["_MALLOC_BENCH_SUBPROCESS"] = "1"
        proc = subprocess.run(
            [python, script],
            env=env,
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            print(f"--- stderr from {allocator} ---")
            print(proc.stderr[-2000:])
            raise RuntimeError(f"Subprocess for {allocator} failed")
        # Print the benchmark table to our stderr
        print(proc.stderr, end="")
        # JSON is on stdout
        return json.loads(proc.stdout.strip())

    print("\n  Running with PYTHONMALLOC=pymalloc ...\n")
    res_pymalloc = run_with("pymalloc")
    print("  Running with PYTHONMALLOC=malloc ...\n")
    res_malloc = run_with("malloc")

    # ── Pretty comparison table ──────────────────────────────────────────
    print(f"\n{'=' * 90}")
    print(f"  COMPARISON: pymalloc vs malloc")
    print(f"  Python: {res_pymalloc.get('python', 'unknown')}")
    print(f"{'=' * 90}\n")
    print(f"  {'':40} {'── wall (ms) ──':^28}  {'── cpu (ms) ──':^28}")
    hdr = (
        f"  {'Benchmark':<40} "
        f"{'pymalloc':>8} {'malloc':>8} {'Δ':>7}  "
        f"{'pymalloc':>8} {'malloc':>8} {'Δ':>7}"
    )
    print(hdr)
    print(f"  {'-'*40} {'-'*8} {'-'*8} {'-'*7}  {'-'*8} {'-'*8} {'-'*7}")

    total_pw = total_mw = total_pc = total_mc = 0.0

    for bp, bm in zip(res_pymalloc["benchmarks"], res_malloc["benchmarks"]):
        pw = bp["wall_mean"] * 1000
        mw = bm["wall_mean"] * 1000
        pc = bp["cpu_mean"] * 1000
        mc = bm["cpu_mean"] * 1000
        total_pw += pw
        total_mw += mw
        total_pc += pc
        total_mc += mc
        dw = ((mw - pw) / pw * 100) if pw else 0
        dc = ((mc - pc) / pc * 100) if pc else 0
        sign_w = "+" if dw >= 0 else ""
        sign_c = "+" if dc >= 0 else ""
        lbl = bp["label"][:40]
        print(
            f"  {lbl:<40} "
            f"{pw:>7.1f}  {mw:>7.1f}  {sign_w}{dw:>5.1f}%  "
            f"{pc:>7.1f}  {mc:>7.1f}  {sign_c}{dc:>5.1f}%"
        )

    # Totals
    dw_total = ((total_mw - total_pw) / total_pw * 100) if total_pw else 0
    dc_total = ((total_mc - total_pc) / total_pc * 100) if total_pc else 0
    sign_tw = "+" if dw_total >= 0 else ""
    sign_tc = "+" if dc_total >= 0 else ""
    print(f"  {'-'*40} {'-'*8} {'-'*8} {'-'*7}  {'-'*8} {'-'*8} {'-'*7}")
    print(
        f"  {'TOTAL':<40} "
        f"{total_pw:>7.1f}  {total_mw:>7.1f}  {sign_tw}{dw_total:>5.1f}%  "
        f"{total_pc:>7.1f}  {total_mc:>7.1f}  {sign_tc}{dc_total:>5.1f}%"
    )

    print()
    print(f"  Δ = how much SLOWER malloc is vs pymalloc  (+N% = pymalloc wins)")
    print(f"  Δ = how much FASTER malloc is vs pymalloc  (-N% = malloc wins)")

    # ── RSS comparison ───────────────────────────────────────────────────
    print(f"\n  Peak RSS: pymalloc = {res_pymalloc['benchmarks'][-1]['rss_kib']:,} KiB"
          f"  |  malloc = {res_malloc['benchmarks'][-1]['rss_kib']:,} KiB")
    print()


# ---------------------------------------------------------------------------
# Entry
# ---------------------------------------------------------------------------

def main():
    if os.environ.get("_MALLOC_BENCH_SUBPROCESS"):
        # Sub-process mode: run benchmarks and emit JSON on stdout
        results = run_benchmarks()
        print(json.dumps(results))
    else:
        run_comparison()


if __name__ == "__main__":
    main()
