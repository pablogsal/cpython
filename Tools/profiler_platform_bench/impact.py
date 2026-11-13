"""Compare target throughput at a fixed requested sampling rate."""
import json
import subprocess
import sys
import threading
import time


def target(count):
    stop = threading.Event()
    barrier = threading.Barrier(count + 1)
    work = [0]
    def busy():
        barrier.wait()
        batches = 0
        while not stop.is_set():
            for i in range(1000):
                pass
            batches += 1
        work[0] = batches
    def idle():
        barrier.wait()
        stop.wait()
    threads = [threading.Thread(target=busy)]
    threads.extend(threading.Thread(target=idle) for _ in range(count - 1))
    for thread in threads:
        thread.start()
    barrier.wait()
    started = time.perf_counter()
    print('READY', flush=True)
    sys.stdin.readline()
    stop.set()
    for thread in threads:
        thread.join()
    print(json.dumps({'batches': work[0], 'seconds': time.perf_counter() - started}), flush=True)


def worker(executable, count, mode):
    import _remote_debugging as remote
    child = subprocess.Popen([executable, __file__, 'target', str(count)],
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    try:
        assert child.stdout.readline().strip() == 'READY'
        unwinder = None if mode == 'control' else remote.RemoteUnwinder(
            child.pid, all_threads=True, cache_frames=True, mode=3,
            skip_non_matching_threads=False)
        calls = missed = failures = unknown = records = on_cpu = 0
        start = time.perf_counter()
        cpu_start = time.process_time()
        deadline = start
        while time.perf_counter() - start < 3:
            if unwinder is not None:
                try:
                    stacks = unwinder.get_stack_trace()
                except (OSError, RuntimeError, UnicodeDecodeError):
                    failures += 1
                else:
                    for interp in stacks:
                        for info in interp.threads:
                            records += 1
                            unknown += bool(info.status & 4)
                            on_cpu += bool(info.status & 2)
                calls += 1
            deadline += .001
            now = time.perf_counter()
            if deadline < now:
                skipped = int((now - deadline) / .001) + 1
                missed += skipped
                deadline += skipped * .001
            time.sleep(max(0, deadline - time.perf_counter()))
        cpu = time.process_time() - cpu_start
    finally:
        output, _ = child.communicate('\n', timeout=30)
    target_result = json.loads(output)
    print(json.dumps({'worker_threads': count, 'mode': mode, 'calls': calls,
          'missed_intervals': missed, 'failures': failures, 'unknown': unknown,
          'records': records, 'on_cpu': on_cpu, 'profiler_cpu_seconds': cpu,
          'target_batches_per_second': target_result['batches'] / target_result['seconds']}))


def compare(baseline, candidate):
    for count in (1, 32):
        for trial in range(5):
            order = [('control', baseline, 'control'), ('baseline', baseline, 'sample'),
                     ('candidate', candidate, 'sample')]
            order = order[trial % 3:] + order[:trial % 3]
            for label, executable, mode in order:
                output = subprocess.check_output([executable, __file__, 'worker',
                          baseline, str(count), mode], text=True)
                row = json.loads(output)
                row.update(build=label, trial=trial)
                print(json.dumps(row), flush=True)


if __name__ == '__main__':
    mode, *args = sys.argv[1:]
    if mode == 'target':
        target(int(args[0]))
    elif mode == 'worker':
        worker(args[0], int(args[1]), args[2])
    else:
        compare(*args)
