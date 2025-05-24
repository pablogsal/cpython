import argparse
import _remote_debugging
import time
import signal
from collections import defaultdict

class SamplingProfiler:
    def __init__(self):
        self.function_stats = defaultdict(lambda: {'ncalls': 0, 'tottime': 0.0})
        self.total_samples = 0
        self.total_sample_time = 0.0
        self.total_processing_time = 0.0
        self.start_time = None
        self.end_time = None

    def add_sample(self, stack_trace, sample_time):
        """Add a stack trace sample and aggregate immediately"""
        process_start = time.perf_counter()

        self.total_samples += 1
        self.total_sample_time += sample_time

        for thread_id, frames in stack_trace:
            if not frames:
                continue

            # Aggregate by function name (not line number)
            for func, file, lineno in frames:
                # Extract just filename and function name
                file_name = file.split('/')[-1] if '/' in file else file
                func_key = f"{file_name}({func})"

                self.function_stats[func_key]['ncalls'] += 1
                self.function_stats[func_key]['tottime'] += sample_time

        process_end = time.perf_counter()
        self.total_processing_time += (process_end - process_start)

    def print_results(self):
        """Print sampling profiler results"""
        if self.total_samples == 0:
            print("No samples collected.")
            return

        wall_time = self.end_time - self.start_time if self.start_time and self.end_time else 0
        total_function_calls = sum(stats['ncalls'] for stats in self.function_stats.values())

        print(f"\nSampling Profiler Results")
        print(f"Total samples: {self.total_samples}")
        print(f"Wall time: {wall_time:.3f} seconds")
        print(f"Sample rate: {self.total_samples/wall_time:.2f} Hz")
        print(f"Sample time: {self.total_sample_time:.3f} seconds")
        print(f"Processing time: {self.total_processing_time:.3f} seconds")
        print(f"Avg sample+processing: {((self.total_sample_time + self.total_processing_time)/self.total_samples)*1e6:.2f} µs")
        print(f"Functions observed: {len(self.function_stats)}")
        print(f"\nOrdered by: sample count (time spent)")
        print()
        print(f"{'samples':<10} {'%time':<8} {'avg_ms':<8} {'sample_hz':<10} function")
        print("-" * 70)

        # Sort by sample count (ncalls) - this shows where most time is spent
        sorted_funcs = sorted(self.function_stats.items(),
                             key=lambda x: x[1]['ncalls'],
                             reverse=True)

        for func_name, stats in sorted_funcs:
            samples = stats['ncalls']
            sample_time = stats['tottime']

            # Calculate meaningful metrics
            percent_time = (samples / total_function_calls) * 100 if total_function_calls > 0 else 0
            avg_sample_time_ms = (sample_time / samples * 1000) if samples > 0 else 0
            sample_rate = samples / wall_time if wall_time > 0 else 0

            print(f"{samples:<10} {percent_time:<7.1f}% {avg_sample_time_ms:<7.3f} "
                  f"{sample_rate:<9.1f} {func_name}")

        print("\nInterpretation:")
        print("- 'samples': Number of times function appeared in stack traces")
        print("- '%time': Percentage of total execution time spent in this function")
        print("- 'avg_ms': Average time to collect a sample when in this function")
        print("- 'sample_hz': How often this function was sampled per second")

# Global profiler instance
profiler = SamplingProfiler()
shutdown_requested = False

def signal_handler(sig, frame):
    global shutdown_requested
    print("\nShutdown requested...")
    shutdown_requested = True

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

def benchmark_raw(pid):
    print("Raw benchmark mode doesn't collect stack traces for profiling.")
    # Keep original raw benchmark logic
    all = 0
    fail = 0
    total_time = 0.0
    un = _remote_debugging.BinaryStackDumper("results.bin", pid)

    try:
        for _ in range(1000):
            all += 1
            t0 = time.perf_counter()
            try:
                un.dump_thread()
            except (OSError, RuntimeError, UnicodeDecodeError) as e:
                print(e)
                fail += 1
            t1 = time.perf_counter()
            total_time += (t1 - t0)
            success = all - fail
            avg_us = (total_time / all) * 1e6 if all else 0.0
            print(f"Average interval: {avg_us:.2f} µs | "
                f"Average interval: {all/total_time:.2f}Hz "
                f"Success rate: {(success / all) * 100:.2f}%")
    finally:
        un.close()

def sample(unwinder, interval_us):
    global profiler, shutdown_requested

    print("Starting profiling sampler... Press Ctrl+C to stop and see results.")
    signal.signal(signal.SIGINT, signal_handler)

    interval_sec = interval_us / 1e6
    all = 0
    fail = 0
    slow = 0

    profiler.start_time = time.perf_counter()

    try:
        while not shutdown_requested:
            all += 1
            t0 = time.perf_counter()
            try:
                stack_trace = unwinder.get_stack_trace()
                t1 = time.perf_counter()
                sample_time = t1 - t0

                if stack_trace:
                    profiler.add_sample(stack_trace, sample_time)

            except (OSError, RuntimeError, UnicodeDecodeError) as e:
                fail += 1
                t1 = time.perf_counter()

            elapsed = t1 - t0
            if elapsed > interval_sec:
                slow += 1

            # Print progress stats every 100 samples
            if all % 100 == 0:
                wall_time = time.perf_counter() - profiler.start_time
                success = all - fail
                sample_rate = profiler.total_samples / wall_time if wall_time > 0 else 0
                avg_sample_processing = ((profiler.total_sample_time + profiler.total_processing_time) / profiler.total_samples * 1e6) if profiler.total_samples > 0 else 0

                print(f"Samples: {profiler.total_samples} | "
                      f"Rate: {sample_rate:.1f}Hz | "
                      f"Success: {(success/all)*100:.1f}% | "
                      f"Avg: {avg_sample_processing:.1f}µs | "
                      f"Slow: {slow}/{all} | "
                      f"Functions: {len(profiler.function_stats)}")

            if all > 1000 and (slow / all) > 0.90:
                raise RuntimeError("More than 90% of samples exceeded the requested interval")

            sleep_time = interval_sec - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)

    except KeyboardInterrupt:
        pass

    profiler.end_time = time.perf_counter()

    # Print sampling statistics
    wall_time = profiler.end_time - profiler.start_time
    success = all - fail

    print(f"\nSampling Summary:")
    print(f"Total sample attempts: {all}")
    print(f"Failed samples: {fail}")
    print(f"Successful samples: {profiler.total_samples}")
    print(f"Success rate: {(success / all) * 100:.2f}%")
    print(f"Wall time: {wall_time:.3f} seconds")
    print(f"Sample rate: {profiler.total_samples/wall_time:.2f} Hz")
    print(f"Slow samples: {slow}/{all} ({(slow / all) * 100:.2f}%)")

    # Print profiling results
    profiler.print_results()

def main():
    parser = argparse.ArgumentParser(description="Remote stack sampler")
    parser.add_argument("pid", type=int, help="PID of the target process")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--benchmark", action="store_true", help="Run in max-speed benchmark mode")
    group.add_argument("--benchmark-raw", action="store_true", help="Run in max-speed raw benchmark mode")
    group.add_argument("--interval", type=float,
                       help="Sampling interval in microseconds (e.g. 1000 for 1ms)")

    args = parser.parse_args()

    if args.benchmark_raw:
        benchmark_raw(args.pid)
    elif args.benchmark:
        unwinder = _remote_debugging.RemoteUnwinder(args.pid, all_threads=False)
        benchmark(unwinder)
    else:
        unwinder = _remote_debugging.RemoteUnwinder(args.pid, all_threads=False)
        sample(unwinder, args.interval)

if __name__ == "__main__":
    main()
