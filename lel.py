import argparse
import sys
import os
import time
from collections import defaultdict

try:
    from _testexternalinspection import get_all_awaited_by
except ImportError:
    print("Error: _testexternalinspection module not available")
    print("This tool requires _testexternalinspection module to inspect Python processes.")
    sys.exit(1)

def format_stack(stack):
    """Format a stack trace list as a human-readable string."""
    if isinstance(stack, list) and stack and isinstance(stack[0], str):
        return " → ".join(stack)
    return "N/A"

def print_task_list(thread_tasks):
    """Print tasks in a flat list format, similar to ps."""
    # Calculate column widths based on data
    task_width = 20
    stack_width = max(20, min(60, os.get_terminal_size().columns - 55))
    awaited_width = max(20, min(30, os.get_terminal_size().columns - stack_width - 25))

    # Print header
    print(f"{'PID':<6} {'TASK_NAME':<{task_width}} {'STACK':<{stack_width}} {'AWAITED_BY':<{awaited_width}}")
    print("-" * (6 + task_width + stack_width + awaited_width + 3))

    # Process each thread's tasks
    for pid, thread_id, tasks in thread_tasks:
        for task_name, awaited_by in tasks:
            # Extract awaiter task names
            awaiter_names = []
            stack_str = "N/A"

            for awaiter in awaited_by:
                if len(awaiter) >= 2:
                    # Extract awaiter task name
                    awaiter_task = awaiter[1]
                    awaiter_names.append(awaiter_task)

                # Extract stack trace if available
                if len(awaiter) >= 1 and isinstance(awaiter[0], list) and len(awaiter[0]) > 0:
                    stack_str = format_stack(awaiter[0])

            awaited_by_str = "None" if not awaiter_names else ", ".join(awaiter_names)

            # Truncate long strings with ellipsis
            if len(task_name) > task_width:
                task_name = task_name[:task_width-3] + "..."
            if len(stack_str) > stack_width:
                stack_str = stack_str[:stack_width-3] + "..."
            if len(awaited_by_str) > awaited_width:
                awaited_by_str = awaited_by_str[:awaited_width-3] + "..."

            print(f"{pid:<6} {task_name:<{task_width}} {stack_str:<{stack_width}} {awaited_by_str:<{awaited_width}}")

def build_dependency_tree(tasks):
    """Build a tree where each task points to tasks it's awaiting."""
    # Map of task -> tasks it's awaited by
    awaited_by_map = {}
    for task_name, awaited_by in tasks:
        awaited_by_map[task_name] = []
        for awaiter in awaited_by:
            if len(awaiter) >= 2:
                awaiter_task = awaiter[1]
                awaited_by_map[task_name].append(awaiter_task)

    dependency_tree = defaultdict(list)
    for task, awaiters in awaited_by_map.items():
        for awaiter in awaiters:
            dependency_tree[awaiter].append(task)

    return dict(dependency_tree)

def find_root_tasks(dependency_tree, tasks):
    """Find tasks that are not awaiting any other task."""
    all_tasks = set(task_name for task_name, _ in tasks)
    all_dependencies = set()
    for deps in dependency_tree.values():
        all_dependencies.update(deps)

    # Root tasks are those that aren't awaiting anything
    root_tasks = all_tasks - all_dependencies
    return root_tasks

def get_task_stack(task_name, tasks):
    """Get the stack trace for a task."""
    for t_name, awaited_by in tasks:
        if t_name == task_name:
            for awaiter in awaited_by:
                if len(awaiter) >= 1 and isinstance(awaiter[0], list) and len(awaiter[0]) > 0:
                    return awaiter[0]
    return []

def print_task_tree(dependency_tree, tasks, current_task, level=0):
    """Print the task tree recursively."""
    indent = " " * level
    stack = get_task_stack(current_task, tasks)
    stack_str = format_stack(stack)

    # Truncate stack if too long
    terminal_width = os.get_terminal_size().columns
    max_stack_width = max(20, terminal_width - level*2 - len(current_task) - 5)
    if len(stack_str) > max_stack_width:
        stack_str = stack_str[:max_stack_width-3] + "..."

    print(f"{indent}├─ {current_task} [{stack_str}]")

    # Print tasks that this task is awaiting
    dependencies = dependency_tree.get(current_task, [])
    for i, dep in enumerate(sorted(dependencies)):
        is_last = (i == len(dependencies) - 1)
        print_task_tree(dependency_tree, tasks, dep, level + 1)

def main():
    parser = argparse.ArgumentParser( description="Show Python async tasks in a process (like 'ps' for Python tasks)")
    parser.add_argument("pid", nargs="*", type=int, help="Process ID(s) to inspect. If none specified, show all Python processes.")
    parser.add_argument("--tree", "-t", action="store_true", help="Display tasks in a tree format")
    parser.add_argument("--interval", "-n", type=float, default=0, help="Updates display every INTERVAL seconds. With no interval, display once and exit.")
    args = parser.parse_args()

    try:
        while True:
            print(f"Python Task Viewer - {time.strftime('%H:%M:%S')}")
            print(f"{'=' * 80}")

            # Get and display tasks for each PID
            thread_tasks = []
            for pid in args.pid:
                try:
                    all_awaited_by = get_all_awaited_by(pid)
                    import pprint
                    pprint.pprint(all_awaited_by)

                    # Process thread tasks
                    for item in all_awaited_by:
                        if len(item) == 2:
                            thread_id, tasks = item
                            if tasks:  # Skip empty task lists
                                thread_tasks.append((pid, thread_id, tasks))
                except Exception as e:
                    print(f"Error inspecting PID {pid}: {e}")

            if not thread_tasks:
                print("No tasks found in the specified process(es).")
                if args.interval <= 0:
                    return 0

            # Display tasks
            if args.tree:
                for pid, thread_id, tasks in thread_tasks:
                    print(f"\nPID {pid} - Thread ID: {thread_id}")
                    print(f"{'-' * 40}")

                    dependency_tree = build_dependency_tree(tasks)
                    root_tasks = find_root_tasks(dependency_tree, tasks)

                    if root_tasks:
                        for root_task in sorted(root_tasks):
                            print_task_tree(dependency_tree, tasks, root_task)
                    else:
                        print("No root tasks found. There might be a circular dependency.")
            else:
                print_task_list(thread_tasks)

            if args.interval <= 0:
                break

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\nExiting...")
        return 0
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    return 0

main()
