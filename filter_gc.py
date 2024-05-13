import sys


def filter_entries(lines):
    filtered_lines = []
    found_py = False
    batch = []
    for line in lines:
        if line.startswith("python"):
            if batch and len(batch) > 1:
                filtered_lines.append("\n")
                filtered_lines.extend(batch[1])
            batch.clear()
            batch.append(line)
        elif "py::" in line:
            batch.append(line)
    if batch and len(batch) > 1:
        filtered_lines.extend(batch[1])

    return filtered_lines


if __name__ == "__main__":
    lines = sys.stdin.readlines()
    filtered_lines = filter_entries(lines)
    for line in filtered_lines:
        sys.stdout.write(line)
