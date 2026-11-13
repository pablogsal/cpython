"""Repeat the noisy Intel cases with two baseline controls per round."""
import json
from pathlib import Path
import statistics
import subprocess
import sys
import time


def probe(pid, mode):
    from _remote_debugging import RemoteUnwinder
    unwinder = RemoteUnwinder(pid, all_threads=True, cache_frames=True, mode=mode,
                              skip_non_matching_threads=False)
    for _ in range(30):
        unwinder.get_stack_trace()
    samples = []
    failures = unknown = 0
    start = time.perf_counter()
    cpu = time.process_time_ns()
    while time.perf_counter() - start < 3:
        before = time.perf_counter_ns()
        try:
            frames = unwinder.get_stack_trace()
        except (OSError, RuntimeError, UnicodeDecodeError):
            failures += 1
        else:
            samples.append(time.perf_counter_ns() - before)
            if mode:
                unknown += sum(bool(thread.status & 4) for interp in frames for thread in interp.threads)
    cpu = time.process_time_ns() - cpu
    samples.sort()
    print(json.dumps({'median_ns': statistics.median(samples),
                      'p95_ns': samples[int(.95 * (len(samples)-1))],
                      'mean_ns': statistics.mean(samples),
                      'cpu_ns_per_call': cpu / (len(samples) + failures),
                      'successes': len(samples), 'failures': failures, 'unknown': unknown}))


def compare(baseline, candidate):
    target_script = str(Path(__file__).with_name('sampling.py'))
    for count, imported, modes in [(128, 0, (0, 1, 3)), (32, 1, (3,))]:
        child = subprocess.Popen([baseline, target_script, 'target', str(count), str(imported)],
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        try:
            assert child.stdout.readline().strip() == 'READY'
            for mode in modes:
                for trial in range(6):
                    order = [('baseline-a', baseline), ('candidate', candidate), ('baseline-b', baseline)]
                    order = order[trial % 3:] + order[:trial % 3]
                    for label, executable in order:
                        output = subprocess.check_output([executable, __file__, 'probe', str(child.pid), str(mode)], text=True)
                        result = json.loads(output)
                        result.update(build=label, worker_threads=count, mode=mode, trial=trial)
                        print(json.dumps(result), flush=True)
        finally:
            child.communicate('\n', timeout=30)


if __name__ == '__main__':
    mode, *args = sys.argv[1:]
    if mode == 'probe':
        probe(int(args[0]), int(args[1]))
    else:
        compare(*args)
