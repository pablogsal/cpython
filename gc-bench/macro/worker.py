import argparse
import gc
import json
import os
import resource
import runpy
import sys
import time
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--output', required=True)
p.add_argument('module')
p.add_argument('args', nargs=argparse.REMAINDER)
a = p.parse_args()
sys.argv = [a.module, *a.args]
sys.path[0] = os.getcwd()

def delta(before, after):
    return [{key: new[key] - old[key] for key in new}
            for old, new in zip(before, after)]

before = gc.get_stats()
start_cpu = time.process_time_ns()
start_wall = time.perf_counter_ns()

def record_result(status, *, hard_exit=False):
    cpu = time.process_time_ns() - start_cpu
    wall = time.perf_counter_ns() - start_wall
    after = gc.get_stats()
    thresholds = gc.get_threshold()
    enabled = gc.isenabled()
    teardown = None
    if not hard_exit:
        teardown_cpu_start = time.process_time_ns()
        teardown_wall_start = time.perf_counter_ns()
        collected = gc.collect()
        teardown_cpu = time.process_time_ns() - teardown_cpu_start
        teardown_wall = time.perf_counter_ns() - teardown_wall_start
        final = gc.get_stats()
        teardown = dict(cpu_s=teardown_cpu/1e9, wall_s=teardown_wall/1e9,
                        collected=collected, gc=delta(after, final))
    record = dict(module=a.module, args=a.args, status=status, cpu_s=cpu/1e9,
                  wall_s=wall/1e9, gc=delta(before, after), teardown=teardown,
                  hard_exit=hard_exit,
                  max_rss_kib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
                  gc_thresholds=thresholds, gc_enabled=enabled,
                  executable=sys.executable, python_version=sys.version)
    Path(a.output).write_text(json.dumps(record, indent=2)+'\n')

original_hard_exit = os._exit

def measured_hard_exit(status):
    record_result(status, hard_exit=True)
    original_hard_exit(status)

os._exit = measured_hard_exit
status = 0
try:
    runpy.run_module(a.module, run_name='__main__', alter_sys=True)
except SystemExit as exc:
    status = exc.code if isinstance(exc.code, int) else 0 if exc.code is None else 1
record_result(status)
sys.exit(status)
