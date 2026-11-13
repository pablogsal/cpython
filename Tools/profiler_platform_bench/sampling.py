"""Compare the same target executable under two profiler builds."""
import json
import statistics
import subprocess
import sys
import threading
import time
from pathlib import Path


def target(count, loaded):
    if loaded:
        import asyncio
    stop = threading.Event()
    threads = [threading.Thread(target=stop.wait) for _ in range(count)]
    for thread in threads:
        thread.start()
    print('READY', flush=True)
    sys.stdin.readline()
    stop.set()
    for thread in threads:
        thread.join()


def worker(target_python, count, loaded):
    import _remote_debugging as remote
    child = subprocess.Popen([target_python, __file__, 'target', str(count), str(int(loaded))],
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    try:
        if child.stdout.readline().strip() != 'READY':
            raise RuntimeError('target failed to initialize')
        results = {}
        # Preserve every thread so a lost state cannot appear as a speedup.
        for name in ('attach', 'wall', 'all', 'cpu', 'async'):
            mode = {'wall': 0, 'all': 3,
                    'cpu': 1}.get(name, 0)
            unwinder = remote.RemoteUnwinder(child.pid, all_threads=True, mode=mode,
                                           skip_non_matching_threads=False, cache_frames=True)
            if name == 'attach':
                action = lambda: remote.RemoteUnwinder(child.pid, all_threads=True, cache_frames=True)
            elif name == 'async':
                action = unwinder.get_all_awaited_by
            else:
                action = unwinder.get_stack_trace
            samples = []
            failures = unknown = records = 0
            for _ in range(5):
                try:
                    action()
                except RuntimeError:
                    if name != 'async' or loaded:
                        raise
            cpu_start = time.process_time_ns()
            start = time.perf_counter()
            while len(samples) < 30 or time.perf_counter() - start < 1.0:
                before = time.perf_counter_ns()
                try:
                    value = action()
                except RuntimeError:
                    if name != 'async' or loaded:
                        raise
                    failures += 1
                    elapsed = time.perf_counter_ns() - before
                else:
                    elapsed = time.perf_counter_ns() - before
                    if name in ('all', 'cpu'):
                        for interp in value:
                            for info in interp.threads:
                                records += 1
                                unknown += bool(info.status & (1 << 2))
                samples.append(elapsed)
            cpu_ns = time.process_time_ns() - cpu_start
            samples.sort()
            results[name] = {'median_ns': statistics.median(samples),
                             'p95_ns': samples[int(.95 * (len(samples)-1))],
                             'cpu_ns_per_call': cpu_ns / len(samples),
                             'calls': len(samples), 'failures': failures,
                             'records': records, 'unknown': unknown}
        print(json.dumps(results), flush=True)
    finally:
        child.communicate('\n', timeout=30)


def compare(baseline, candidate):
    results = []
    for count in (1, 32, 128):
        for loaded in (False, True):
            for trial in range(5):
                order = [('baseline', baseline), ('candidate', candidate)]
                if trial % 2:
                    order.reverse()
                for label, executable in order:
                    output = subprocess.check_output([executable, __file__, 'worker',
                               baseline, str(count), str(int(loaded))], text=True)
                    row = {'build': label, 'worker_threads': count, 'asyncio_imported': loaded,
                           'trial': trial, 'measurements': json.loads(output)}
                    results.append(row)
                    print(json.dumps(row), flush=True)
    Path('sampling-results.json').write_text(json.dumps(results, indent=2))


if __name__ == '__main__':
    mode, *args = sys.argv[1:]
    if mode == 'target':
        target(int(args[0]), bool(int(args[1])))
    elif mode == 'worker':
        worker(args[0], int(args[1]), bool(int(args[2])))
    else:
        compare(*args)
