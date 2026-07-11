#!/usr/bin/env python3
import argparse
import base64
import io
import json
import random
import subprocess
import sys
import tokenize
from pathlib import Path

import _tokenize


def exception_result(exc):
    return {
        "type": type(exc).__name__,
        "message": getattr(exc, "msg", str(exc)),
        "lineno": getattr(exc, "lineno", None),
        "offset": getattr(exc, "offset", None),
        "end_lineno": getattr(exc, "end_lineno", None),
        "end_offset": getattr(exc, "end_offset", None),
    }


def tokenize_source(source, extra_tokens):
    iterator = _tokenize.TokenizerIter(
        io.StringIO(source).readline,
        extra_tokens=extra_tokens,
    )
    tokens = []
    try:
        for item in iterator:
            token_type, text, start, end, line = item
            tokens.append([token_type, text, start, end, line])
    except BaseException as exc:
        return {"tokens": tokens, "error": exception_result(exc)}
    return {"tokens": tokens, "error": None}


def tokenize_bytes(data):
    tokens = []
    try:
        for item in tokenize.tokenize(io.BytesIO(data).readline):
            tokens.append([
                item.type,
                item.string,
                item.start,
                item.end,
                item.line,
            ])
    except BaseException as exc:
        return {"tokens": tokens, "error": exception_result(exc)}
    return {"tokens": tokens, "error": None}


def compile_source(source):
    try:
        compile(source, "<difftest>", "exec")
    except BaseException as exc:
        return exception_result(exc)
    return None


def worker():
    cases = json.load(sys.stdin)
    results = []
    for case in cases:
        if "path" in case:
            data = Path(case["path"]).read_bytes()
        else:
            data = base64.b64decode(case["data"])
        source = data.decode("utf-8", "surrogateescape")
        result = {
            "name": case["name"],
            "parser": compile_source(data),
            "bytes_tokens": tokenize_bytes(data),
        }
        for extra_tokens in (False, True):
            key = "extra" if extra_tokens else "parser_tokens"
            try:
                result[key] = tokenize_source(source, extra_tokens)
            except BaseException as exc:
                result[key] = {"tokens": [], "error": exception_result(exc)}
        results.append(result)
    json.dump(results, sys.stdout, ensure_ascii=True, separators=(",", ":"))


def inline_case(name, data):
    return {
        "name": name,
        "data": base64.b64encode(data).decode("ascii"),
    }


def case_source(case):
    if "path" in case:
        return None
    data = base64.b64decode(case["data"])
    if len(data) > 512:
        data = data[:512] + b"..."
    return repr(data)


def corpus_cases(root, limit):
    paths = sorted((root / "Lib").rglob("*.py"))
    if limit:
        paths = paths[:limit]
    return [{"name": str(path.relative_to(root)), "path": str(path)}
            for path in paths]


def adversarial_cases(seed, mutations):
    sources = [
        b"x = 1\n",
        b"f'{x:{y}}'\n",
        b"f'{1:{2}{{3}}}'\n",
        b"t'{x=}'\n",
        b"x = '''unterminated",
        b"x = \\\n",
        b"# coding: latin-1\nvalue = '\xe9'\n",
        b"\xef\xbb\xbf# coding: utf-8\nvalue = 1\n",
        "é = (\n".encode(),
        b"if x:\r\n\tpass\r\n",
        b"a\x00b\n",
        b"x = \xc3\xa9\xff\n",
    ]
    rng = random.Random(seed)
    alphabet = b"()[]{}'\"ftr012_:=!\\\n #abc\x80\xff"
    for _ in range(mutations):
        source = bytearray(rng.choice(sources))
        for __ in range(rng.randint(1, 4)):
            operation = rng.randrange(3)
            index = rng.randrange(len(source) + 1)
            if operation == 0 or not source:
                source[index:index] = bytes([rng.choice(alphabet)])
            elif operation == 1:
                del source[min(index, len(source) - 1)]
            else:
                source[min(index, len(source) - 1)] = rng.choice(alphabet)
        sources.append(bytes(source))
    return [inline_case(f"adversarial-{index}", source)
            for index, source in enumerate(sources)]


def run_worker(interpreter, script, cases):
    process = subprocess.run(
        [interpreter, script, "--worker"],
        input=json.dumps(cases),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if process.returncode:
        raise RuntimeError(
            f"{interpreter} worker failed ({process.returncode}):\n"
            f"{process.stderr}"
        )
    return json.loads(process.stdout)


def first_difference(left, right, path=""):
    if type(left) is not type(right):
        return path, left, right
    if isinstance(left, dict):
        if left.keys() != right.keys():
            return path, sorted(left), sorted(right)
        for key in left:
            difference = first_difference(
                left[key], right[key], f"{path}.{key}" if path else key)
            if difference is not None:
                return difference
        return None
    if isinstance(left, list):
        if len(left) != len(right):
            return f"{path}.length", len(left), len(right)
        for index, (left_item, right_item) in enumerate(zip(left, right)):
            difference = first_difference(
                left_item, right_item, f"{path}[{index}]")
            if difference is not None:
                return difference
        return None
    if left != right:
        return path, left, right
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline")
    parser.add_argument("--candidate", default=sys.executable)
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[2])
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--mutations", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--max-differences", type=int, default=20)
    parser.add_argument("--batch-size", type=int, default=25)
    parser.add_argument("--worker", action="store_true")
    args = parser.parse_args()
    if args.worker:
        worker()
        return 0
    if args.baseline is None:
        parser.error("--baseline is required")

    cases = corpus_cases(args.root, args.limit)
    cases.extend(adversarial_cases(args.seed, args.mutations))
    script = str(Path(__file__).resolve())
    differences = 0
    compared = 0
    for offset in range(0, len(cases), args.batch_size):
        batch = cases[offset:offset + args.batch_size]
        baseline = run_worker(args.baseline, script, batch)
        candidate = run_worker(args.candidate, script, batch)
        for case, left, right in zip(batch, baseline, candidate, strict=True):
            compared += 1
            if left == right:
                continue
            differences += 1
            path, left_value, right_value = first_difference(left, right)
            print(f"{left['name']}: {path}")
            print(f"  baseline:  {left_value!r}")
            print(f"  candidate: {right_value!r}")
            source = case_source(case)
            if source is not None:
                print(f"  source:    {source}")
            if differences >= args.max_differences:
                break
        if differences >= args.max_differences:
            break
    print(f"compared {compared} cases; differences: {differences}")
    return differences != 0


if __name__ == "__main__":
    raise SystemExit(main())
