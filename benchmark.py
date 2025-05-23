import argparse
import _remote_debugging
import time

def benchmark(unwinder):
    all = 0
    fail = 0
    total_time = 0.0

    while True:
        all += 1
        t0 = time.perf_counter()
        n_frames = 1
        try:
            n_frames = len(unwinder.get_stack_trace()[0][1]) + 1
        except (OSError, RuntimeError, UnicodeDecodeError) as e:
            print(e)
            fail += 1
        t1 = time.perf_counter()

        total_time += (t1 - t0)
        success = all - fail

        avg_us = (total_time / all) * 1e6 if all else 0.0
        print(f"Average interval: {avg_us:.2f} µs | "
            f"Average interval: {all/total_time:.2f}Hz "
            f"Average interval per frame: {avg_us/n_frames:.2f} µs | "
            f"Success rate: {(success / all) * 100:.2f}%")


def sample(unwinder, interval_us):
    interval_sec = interval_us / 1e6
    all = 0
    fail = 0
    slow = 0

    while True:
        all += 1
        t0 = time.perf_counter()
        try:
            unwinder.get_stack_trace()
        except (RuntimeError, UnicodeDecodeError):
            fail += 1
        t1 = time.perf_counter()

        elapsed = t1 - t0
        if elapsed > interval_sec:
            slow += 1

        success = all - fail
        print(f"Interval: {interval_us:.2f} µs | "
              f"Sample rate: {(elapsed/1e-6):.2f}us | "
              f"Success rate: {(success / all) * 100:.2f}% | "
              f"Slow samples: {slow}/{all} ({(slow / all) * 100:.2f}%)")

        if all > 1000 and (slow / all) > 0.90:
            raise RuntimeError("More than 90% of samples exceeded the requested interval")

        sleep_time = interval_sec - elapsed
        if sleep_time > 0:
            time.sleep(sleep_time)


def main():
    parser = argparse.ArgumentParser(description="Remote stack sampler")
    parser.add_argument("pid", type=int, help="PID of the target process")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--benchmark", action="store_true", help="Run in max-speed benchmark mode")
    group.add_argument("--interval", type=float,
                       help="Sampling interval in microseconds (e.g. 1000 for 1ms)")

    args = parser.parse_args()
    unwinder = _remote_debugging.RemoteUnwinder(args.pid, all_threads=False)

    if args.benchmark:
        benchmark(unwinder)
    else:
        sample(unwinder, args.interval)


if __name__ == "__main__":
    main()

