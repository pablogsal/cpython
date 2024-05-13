import sys
import re
import collections
import statistics

# Regular expressions to extract relevant information
drop_gil_pattern = re.compile(
    r'python\s+(\d+)\s+\[\d+\]\s+([\d.]+):\s+python:drop_gil: \((\w+)\)')
drop_gil_return_pattern = re.compile(
    r'python\s+(\d+)\s+\[\d+\]\s+([\d.]+):\s+python:drop_gil__return: \((\w+) <- (\w+)\)')

# Initialize variables to store drop_gil data
current_pid = None
start_timestamp = None
call_site = None
pattern = re.compile(r'py::(.*?):')
entries = []

# Read input from stdin
for line in sys.stdin:
    # Match drop_gil lines
    drop_gil_match = drop_gil_pattern.match(line)
    if drop_gil_match:
        pid, timestamp, addr = drop_gil_match.groups()
        current_pid = pid
        start_timestamp = timestamp
        continue

    # Match drop_gil__return lines
    drop_gil_return_match = drop_gil_return_pattern.match(line)
    if drop_gil_return_match:
        pid, timestamp, _, return_addr = drop_gil_return_match.groups()
        if pid == current_pid:
            # Calculate time spent waiting for GIL
            wait_time = float(timestamp) - float(start_timestamp)
            if call_site is not None:
                entries.append((call_site, wait_time))
        # Reset variables for the next iteration
        current_pid = None
        start_timestamp = None
        call_site = None
        continue

    # Extract call site name
    if current_pid and not call_site and line.strip() and "py::" in line:
        line = line.strip()
        m = pattern.search(line)
        if m:
            call_site = m.group(1) or line.strip()
        else:
            call_site = line.strip()

# Sort entries by wait time
sorted_entries = sorted(entries, key=lambda x: x[1], reverse=True)

for entry in tuple(sorted_entries)[:10]:
    print(
        f"Call site: {entry[0]}, Time spent waiting for GIL: {entry[1]*1000:.2f} milliseconds")

# # Group entries by call site
# grouped_entries = collections.defaultdict(list)
# for entry in sorted_entries:
#     call_site = entry[0]
#     grouped_entries[call_site].append(entry[1])

# entries = {}
# for call_site, stuff in grouped_entries.items():
#     entries[call_site] = statistics.mean(stuff)

# Print sorted entries
# for entry in tuple(entries.items())[:10]:
#     print(
#         f"Call site: {entry[0]}, Time spent waiting for GIL: {entry[1]*1000:.2f} milliseconds")
