# perf record -F 9999 -g -o perf.data -e python:drop_gil -e python:drop_gil__return -- ./python -Xperf -m test test_asyncio -v
perf record -F 9999 -g -o perf.data -e python:gc_collect_main -- ./python -Xperf -m test test_asyncio -v
perf script -i perf.data | python filter.py | ~/github/jit_example//FlameGraph/stackcollapse-perf.pl > out.perf-folded
cat out.perf-folded| ~/github/jit_example/FlameGraph/flamegraph.pl > perf.svg
