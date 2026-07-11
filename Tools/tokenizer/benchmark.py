#!/usr/bin/env python3
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import tokenize

try:
    import resource
except ImportError:
    resource = None


def peak_rss_mib():
    if resource is None:
        return None
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if sys.platform == "darwin":
        return rss / (1024 * 1024)
    return rss / 1024


def consume_parse(paths, repeat):
    size = 0
    started = time.perf_counter()
    for _ in range(repeat):
        for path in paths:
            data = Path(path).read_bytes()
            size += len(data)
            try:
                compile(data, path, "exec")
            except (SyntaxError, ValueError, OverflowError):
                pass
    return size, time.perf_counter() - started


def consume_tokenize(paths, repeat):
    size = 0
    started = time.perf_counter()
    for _ in range(repeat):
        for path in paths:
            size += os.path.getsize(path)
            with open(path, "rb") as source:
                try:
                    for _ in tokenize.tokenize(source.readline):
                        pass
                except (IndentationError, SyntaxError, tokenize.TokenError):
                    pass
    return size, time.perf_counter() - started


def worker(mode):
    request = json.load(sys.stdin)
    paths = request["paths"]
    repeat = request.get("repeat", 1)
    if mode == "parse":
        size, elapsed = consume_parse(paths, repeat)
    else:
        size, elapsed = consume_tokenize(paths, repeat)
    json.dump({
        "bytes": size,
        "seconds": elapsed,
        "mib_per_second": size / elapsed / (1024 * 1024),
        "peak_rss_mib": peak_rss_mib(),
    }, sys.stdout)


def run_worker(interpreter, script, mode, paths, repeat=1):
    process = subprocess.run(
        [interpreter, script, "--worker", mode],
        input=json.dumps({"paths": paths, "repeat": repeat}),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if process.returncode:
        raise RuntimeError(
            f"{interpreter} {mode} benchmark failed:\n{process.stderr}"
        )
    return json.loads(process.stdout)


def synthetic_source(path, size_mib):
    target = size_mib * 1024 * 1024
    block = b"value = 1\n" * (1024 * 1024 // 10)
    written = 0
    with open(path, "wb") as output:
        while written < target:
            chunk = block[:min(len(block), target - written)]
            output.write(chunk)
            written += len(chunk)


def format_result(name, mode, result):
    throughput = result["mib_per_second"]
    rss = result["peak_rss_mib"]
    rss_text = "n/a" if rss is None else f"{rss:.1f} MiB"
    seconds = result["seconds"]
    print(
        f"{name:12} {mode:9} {throughput:9.1f} MiB/s  "
        f"{rss_text:>13} peak RSS  {seconds:7.3f} s"
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--candidate", default=sys.executable)
    parser.add_argument("--baseline")
    parser.add_argument(
        "--root", type=Path,
        default=Path(__file__).resolve().parents[2],
    )
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--rss-size-mib", type=int, default=100)
    parser.add_argument("--worker", choices=("parse", "tokenize"))
    args = parser.parse_args()
    if args.worker is not None:
        worker(args.worker)
        return 0

    paths = [str(path) for path in sorted((args.root / "Lib").rglob("*.py"))]
    if args.limit:
        paths = paths[:args.limit]
    script = str(Path(__file__).resolve())
    interpreters = [("candidate", args.candidate)]
    if args.baseline is not None:
        interpreters.insert(0, ("baseline", args.baseline))

    for name, interpreter in interpreters:
        for mode in ("parse", "tokenize"):
            result = run_worker(
                interpreter, script, mode, paths, args.repeat)
            format_result(name, mode, result)

    with tempfile.TemporaryDirectory() as directory:
        path = os.path.join(directory, "tokenizer-rss.py")
        synthetic_source(path, args.rss_size_mib)
        for name, interpreter in interpreters:
            result = run_worker(interpreter, script, "tokenize", [path])
            format_result(name, "rss", result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
