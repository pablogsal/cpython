"""Run each diagnostic independently so one failure does not hide other cases."""

from pathlib import Path
import os
import subprocess
import sys


def main():
    variant = sys.argv[1]
    scripts = Path(__file__).resolve().parent
    results = Path("results")
    results.mkdir(exist_ok=True)
    summaries = [f"\n### {variant}\n"]
    failed = False
    for target in (None, "direct_target.py", "callback_target.py"):
        for cache in (True, False):
            name = Path(target).stem if target else "short_calls"
            name = f"{variant}-{name}-cache-{cache}"
            command = [sys.executable, str(scripts / "diagnose.py"), "--seconds", "3"]
            if target:
                command += ["--target", str(scripts / target)]
            if not cache:
                command.append("--no-cache")
            print(f"\n{name}", flush=True)
            try:
                run = subprocess.run(command, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True,
                                     encoding="utf-8", errors="replace", timeout=90)
                output = run.stdout
                failed |= run.returncode != 0
                summaries.append(f"\n{name} (exit {run.returncode})\n")
            except subprocess.TimeoutExpired as exc:
                output = exc.stdout or b""
                if isinstance(output, bytes):
                    output = output.decode("utf-8", errors="replace")
                output += "\nDiagnostic timed out after 90 seconds.\n"
                failed = True
                summaries.append(f"\n{name}: timed out\n")
            print(output, flush=True)
            (results / f"{name}.txt").write_text(output, encoding="utf-8")
            summaries.extend(line + "\n" for line in output.splitlines()
                             if line.startswith(("live:", "pause:", "delay:", "context:")))
    summary = "".join(summaries)
    (results / f"{variant}-summary.txt").write_text(summary, encoding="utf-8")
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as stream:
            stream.write(summary)
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
