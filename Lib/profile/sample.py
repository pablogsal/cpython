import collections
import marshal
import pstats
import time
import _remote_debugging
import argparse


class SampleProfile:
    def __init__(self, pid, sample_interval_usec, all_threads):
        self.pid = pid
        self.sample_interval_usec = sample_interval_usec
        self.all_threads = all_threads
        self.unwinder = _remote_debugging.RemoteUnwinder(
            self.pid, all_threads=self.all_threads
        )
        self.stats = {}
        self.callers = collections.defaultdict(
            lambda: collections.defaultdict(int)
        )

    def sample(self, duration_sec=10):
        result = collections.defaultdict(
            lambda: dict(total_calls=0, total_rec_calls=0, inline_calls=0)
        )
        sample_interval_sec = self.sample_interval_usec / 1_000_000

        running_time = 0
        num_samples = 0
        errors = 0
        start_time = next_time = time.perf_counter()
        while running_time < duration_sec:
            if next_time < time.perf_counter():
                try:
                    stack_frames = self.unwinder.get_stack_trace()
                    self.aggregate_stack_frames(result, stack_frames)
                except RuntimeError, UnicodeDecodeError, OSError:
                    errors += 1

                num_samples += 1
                next_time += sample_interval_sec

            running_time = time.perf_counter() - start_time

        print(f"Captured {num_samples} samples in {running_time:.2f} seconds")
        print(f"Sample rate: {num_samples / running_time:.2f} samples/sec")
        print(f"Error rate: {(errors / num_samples) * 100:.2f}%")

        expected_samples = int(duration_sec / sample_interval_sec)
        if num_samples < expected_samples:
            print(
                f"Warning: missed {expected_samples - num_samples} samples "
                f"from the expected total of {expected_samples} "
                f"({(expected_samples - num_samples) / expected_samples * 100:.2f}%)"
            )

        self.stats = self.convert_to_pstats(result)

    def print_stats(self, sort=-1):
        if not isinstance(sort, tuple):
            sort = (sort,)
        pstats.SampledStats(self).strip_dirs().sort_stats(*sort).print_stats()

    def dump_stats(self, file):
        with open(file, "wb") as f:
            marshal.dump(self.stats, f)

    # Needed for compatibility with pstats.Stats
    def create_stats(self):
        pass

    def convert_to_pstats(self, raw_results):
        sample_interval_sec = self.sample_interval_usec / 1_000_000
        pstats = {}
        callers = {}
        for fname, call_counts in raw_results.items():
            total = call_counts["inline_calls"] * sample_interval_sec
            cumulative = call_counts["total_calls"] * sample_interval_sec
            callers = dict(self.callers.get(fname, {}))
            pstats[fname] = (
                call_counts["total_calls"],
                call_counts["total_rec_calls"]
                if call_counts["total_rec_calls"]
                else call_counts["total_calls"],
                total,
                cumulative,
                callers,
            )

        return pstats

    def aggregate_stack_frames(self, result, stack_frames):
        for thread_id, frames in stack_frames:
            if not frames:
                continue
            top_location = frames[0]
            result[top_location]["inline_calls"] += 1
            result[top_location]["total_calls"] += 1

            for i in range(1, len(frames)):
                callee = frames[i - 1]
                caller = frames[i]
                self.callers[callee][caller] += 1

            if len(frames) <= 1:
                continue

            for location in frames[1:]:
                result[location]["total_calls"] += 1
                if top_location == location:
                    result[location]["total_rec_calls"] += 1


def sample(
    pid,
    *,
    sort=-1,
    sample_interval_usec=100,
    duration_sec=10,
    filename=None,
    all_threads=False,
):
    profile = SampleProfile(pid, sample_interval_usec, all_threads=False)
    profile.sample(duration_sec)
    if filename:
        profile.dump_stats(filename)
    else:
        profile.print_stats(sort)


def main():
    parser = argparse.ArgumentParser(
        description="Sample a process's stack frames.", color=True
    )
    parser.add_argument("pid", type=int, help="Process ID to sample.")
    parser.add_argument(
        "-i",
        "--interval",
        type=int,
        default=10,
        help="Sampling interval in microseconds (default: 10 usec)",
    )
    parser.add_argument(
        "-d",
        "--duration",
        type=int,
        default=10,
        help="Sampling duration in seconds (default: 10 seconds)",
    )
    parser.add_argument(
        "-a",
        "--all-threads",
        action="store_true",
        help="Sample all threads in the process",
    )
    parser.add_argument("-o", "--outfile", help="Save stats to <outfile>")
    args = parser.parse_args()

    sample(
        args.pid,
        sample_interval_usec=args.interval,
        duration_sec=args.duration,
        filename=args.outfile,
        all_threads=args.all_threads,
    )


if __name__ == "__main__":
    main()
