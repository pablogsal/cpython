import argparse
import _remote_debugging
import time
import signal
import select
import os
from collections import defaultdict

class SamplingProfiler:
    def __init__(self, include_line_numbers=True):
        self.function_stats = defaultdict(lambda: {'sample_count': 0, 'line_samples': defaultdict(int)})
        self.total_samples = 0
        self.total_work_time = 0.0  # Time spent sampling + processing
        self.start_time = None
        self.end_time = None
        self.include_line_numbers = include_line_numbers

    def process_stack_trace(self, stack_trace):
        """Process a stack trace sample"""
        self.total_samples += 1

        for thread_id, frames in stack_trace:
            if not frames:
                continue

            # Count each function in the stack trace
            for func, file, lineno in frames:
                # Extract just filename and function name
                file_name = file.split('/')[-1] if '/' in file else file
                func_key = f"{file_name}({func})"
                self.function_stats[func_key]['sample_count'] += 1

                # Track line number samples if enabled
                if self.include_line_numbers and lineno is not None:
                    self.function_stats[func_key]['line_samples'][lineno] += 1

    def print_results(self, show_lines=True, max_lines_per_function=5):
        """Print sampling profiler results"""
        if self.total_samples == 0:
            print("No samples collected.")
            return

        wall_time = self.end_time - self.start_time if self.start_time and self.end_time else 0
        total_function_samples = sum(stats['sample_count'] for stats in self.function_stats.values())

        print(f"\nSampling Profiler Results")
        print(f"Total samples: {self.total_samples}")
        print(f"Wall time: {wall_time:.3f} seconds")
        print(f"Work time: {self.total_work_time:.3f} seconds")
        print(f"Work rate: {self.total_samples/self.total_work_time:.2f} Hz")
        print(f"Average work time: {(self.total_work_time/self.total_samples)*1e6:.2f} µs")

        # Sort functions by sample count
        sorted_functions = sorted(self.function_stats.items(),
                                key=lambda x: x[1]['sample_count'],
                                reverse=True)

        print(f"\n{'samples':<10} {'%time':<7} {'sample_hz':<9} function")
        print("-" * 60)

        for func_name, stats in sorted_functions:
            samples = stats['sample_count']
            percent_time = (samples / total_function_samples) * 100 if total_function_samples > 0 else 0
            sample_rate = samples / wall_time if wall_time > 0 else 0

            print(f"{samples:<10} {percent_time:<7.1f}% {sample_rate:<9.1f} {func_name}")

            # Show line number breakdown if enabled and available
            if show_lines and self.include_line_numbers and stats['line_samples']:
                # Sort lines by sample count
                sorted_lines = sorted(stats['line_samples'].items(),
                                    key=lambda x: x[1],
                                    reverse=True)

                # Show top N lines for this function
                lines_shown = 0
                for line_no, line_samples in sorted_lines:
                    if lines_shown >= max_lines_per_function:
                        remaining = len(sorted_lines) - lines_shown
                        if remaining > 0:
                            print(f"{'':>12}    ... and {remaining} more lines")
                        break

                    line_percent = (line_samples / samples) * 100
                    line_rate = line_samples / wall_time if wall_time > 0 else 0
                    print(f"{'':>12} L{line_no}: {line_samples} samples ({line_percent:.1f}%, {line_rate:.1f}Hz)")
                    lines_shown += 1

        print("\nInterpretation:")
        print("- 'samples': Number of times function appeared in stack traces")
        print("- '%time': Percentage of samples containing this function")
        print("- 'sample_hz': How often this function was sampled per second")
        if show_lines:
            print("- Line numbers show where within each function samples were taken")

    def print_hottest_lines(self, top_n=20):
        """Print the hottest individual lines across all functions"""
        all_lines = []

        for func_name, stats in self.function_stats.items():
            for line_no, line_samples in stats['line_samples'].items():
                all_lines.append((func_name, line_no, line_samples))

        if not all_lines:
            print("\nNo line number data available.")
            return

        # Sort by sample count
        all_lines.sort(key=lambda x: x[2], reverse=True)

        wall_time = self.end_time - self.start_time if self.start_time and self.end_time else 0
        total_function_samples = sum(stats['sample_count'] for stats in self.function_stats.values())

        print(f"\nHottest Lines (Top {min(top_n, len(all_lines))}):")
        print(f"{'samples':<8} {'%time':<7} {'line_hz':<9} function:line")
        print("-" * 65)

        for func_name, line_no, line_samples in all_lines[:top_n]:
            percent_time = (line_samples / total_function_samples) * 100 if total_function_samples > 0 else 0
            line_rate = line_samples / wall_time if wall_time > 0 else 0
            print(f"{line_samples:<8} {percent_time:<7.1f}% {line_rate:<9.1f} {func_name}:{line_no}")

# Global profiler instance
profiler = SamplingProfiler()
shutdown_requested = False

def signal_handler(sig, frame):
    global shutdown_requested
    print("\nShutdown requested...")
    shutdown_requested = True

def benchmark(unwinder):
    """Benchmark mode - measure raw sampling speed"""
    sample_count = 0
    fail_count = 0
    total_work_time = 0.0

    print("Benchmarking sampling speed...")

    while True:
        work_start = time.perf_counter()
        try:
            stack_trace = unwinder.get_stack_trace()
            if stack_trace:
                sample_count += 1
        except (OSError, RuntimeError, UnicodeDecodeError) as e:
            fail_count += 1

        work_end = time.perf_counter()
        total_work_time += (work_end - work_start)

        total_attempts = sample_count + fail_count
        if total_attempts % 10000 == 0:
            avg_work_time_us = (total_work_time / total_attempts) * 1e6
            work_rate = total_attempts / total_work_time if total_work_time > 0 else 0
            success_rate = (sample_count / total_attempts) * 100

            print(f"Attempts: {total_attempts} | "
                  f"Success: {success_rate:.1f}% | "
                  f"Rate: {work_rate:.1f}Hz | "
                  f"Avg: {avg_work_time_us:.2f}µs")

def sample(unwinder, interval_us, show_line_details=True):
    """Main sampling profiler"""
    global profiler, shutdown_requested

    print("Starting profiling sampler... Press Ctrl+C to stop and see results.")
    signal.signal(signal.SIGINT, signal_handler)

    interval_sec = interval_us / 1e6
    sample_attempts = 0
    failed_samples = 0
    slow_samples = 0

    profiler.start_time = time.perf_counter()

    try:
        loop_time = 0
        while not shutdown_requested:
            sample_attempts += 1

            # Measure work time (sampling + processing)
            work_start = time.perf_counter()

            try:
                stack_trace = unwinder.get_stack_trace()
                if stack_trace:
                    profiler.process_stack_trace(stack_trace)

            except (OSError, RuntimeError, UnicodeDecodeError) as e:
                failed_samples += 1

            work_end = time.perf_counter()
            work_time = work_end - work_start
            profiler.total_work_time += work_time

            # Track slow samples
            if work_time > interval_sec:
                slow_samples += 1

            # Progress update every 100 attempts
            if sample_attempts % 100 == 0:
                wall_time = time.perf_counter() - profiler.start_time
                success_rate = ((sample_attempts - failed_samples) / sample_attempts) * 100
                avg_work_time_us = (profiler.total_work_time / sample_attempts) * 1e6
                work_rate = sample_attempts / profiler.total_work_time if profiler.total_work_time > 0 else 0

                print(f"Samples: {profiler.total_samples} | "
                      f"Rate: {work_rate:.1f}Hz | "
                      f"Success: {success_rate:.1f}% | "
                      f"Avg: {avg_work_time_us:.1f}µs | "
                      f"Slow: {slow_samples}/{sample_attempts} | "
                      f"Work time: {work_time/1e-6:.3f}µs | "
                      f"Loop time: {loop_time/1e-6:.3f}µs | "
                      f"Functions: {len(profiler.function_stats)}")

            # Exit if too many slow samples
            if sample_attempts > 20000 and (slow_samples / sample_attempts) > 0.90:
                raise RuntimeError("More than 90% of samples exceeded the requested interval")

            # Sleep to maintain sampling interval
            sleep_time = interval_sec - work_time
            # For some reason sleeping causes the CPU to go slower and the profiler to be less accurate
            if sleep_time > 0:
                start = time.perf_counter()
                while time.perf_counter() - start < sleep_time:
                    os.sched_yield()
            loop_time = time.perf_counter() - work_start

    except KeyboardInterrupt:
        pass

    profiler.end_time = time.perf_counter()

    # Final statistics
    wall_time = profiler.end_time - profiler.start_time
    successful_samples = sample_attempts - failed_samples
    success_rate = (successful_samples / sample_attempts) * 100 if sample_attempts > 0 else 0
    avg_work_time_us = (profiler.total_work_time / sample_attempts) * 1e6 if sample_attempts > 0 else 0
    work_rate = sample_attempts / profiler.total_work_time if profiler.total_work_time > 0 else 0

    print(f"\nSampling Summary:")
    print(f"Sample attempts: {sample_attempts}")
    print(f"Successful samples: {successful_samples}")
    print(f"Failed samples: {failed_samples}")
    print(f"Success rate: {success_rate:.2f}%")
    print(f"Wall time: {wall_time:.3f} seconds")
    print(f"Work time: {profiler.total_work_time:.3f} seconds")
    print(f"Work rate: {work_rate:.2f} Hz")
    print(f"Average work time: {avg_work_time_us:.2f} µs")
    print(f"Slow samples: {slow_samples}/{sample_attempts} ({(slow_samples/sample_attempts)*100:.2f}%)")

    # Print profiling results
    profiler.print_results(show_lines=show_line_details)

    # Also print hottest individual lines
    profiler.print_hottest_lines()

def main():
    parser = argparse.ArgumentParser(description="Remote stack sampler", color=True)
    parser.add_argument("pid", type=int, help="PID of the target process")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--benchmark", action="store_true", help="Run in max-speed benchmark mode")
    group.add_argument("--interval", type=float,
                       help="Sampling interval in microseconds (e.g. 1000 for 1ms)")
    parser.add_argument("--no-lines", action="store_true",
                       help="Disable line number tracking and display")

    args = parser.parse_args()

    global profiler
    profiler = SamplingProfiler(include_line_numbers=not args.no_lines)

    if args.benchmark:
        unwinder = _remote_debugging.RemoteUnwinder(args.pid, all_threads=False)
        benchmark(unwinder)
    else:
        unwinder = _remote_debugging.RemoteUnwinder(args.pid, all_threads=False)
        sample(unwinder, args.interval, show_line_details=not args.no_lines)

if __name__ == "__main__":
    main()
