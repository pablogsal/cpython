import argparse
from collections import defaultdict
from itertools import count
from enum import Enum
from _testexternalinspection import get_all_awaited_by

class NodeType(Enum):
    COROUTINE = 1
    TASK = 2


# ─── indexing helpers ───────────────────────────────────────────
def _index(result):
    id2name, awaits = {}, []
    for _thr_id, tasks in result:
        for tid, tname, awaited in tasks:
            id2name[tid] = tname
            for stack, parent_id in awaited:
                awaits.append((parent_id, stack, tid))
    return id2name, awaits


def _build_tree(id2name, awaits):
    id2label = {(NodeType.TASK, tid): name for tid, name in id2name.items()}
    children = defaultdict(list)
    cor_names = defaultdict(dict)  # (parent) -> {frame: node}
    cor_id_seq = count(1)

    def _cor_node(parent_key, frame_name):
        """Return an existing or new (NodeType.COROUTINE, …) node under *parent_key*."""
        bucket = cor_names[parent_key]
        if frame_name in bucket:
            return bucket[frame_name]
        node_key = (NodeType.COROUTINE, f"c{next(cor_id_seq)}")
        id2label[node_key] = frame_name
        children[parent_key].append(node_key)
        bucket[frame_name] = node_key
        return node_key

    # touch every task so it’s present even if it awaits nobody
    for tid in id2name:
        children[(NodeType.TASK, tid)]

    # lay down parent ➜ …frames… ➜ child paths
    for parent_id, stack, child_id in awaits:
        cur = (NodeType.TASK, parent_id)
        for frame in reversed(stack):  # outer-most → inner-most
            cur = _cor_node(cur, frame)
        child_key = (NodeType.TASK, child_id)
        if child_key not in children[cur]:
            children[cur].append(child_key)

    return id2label, children


def _roots(id2label, children):
    roots = [n for n, lbl in id2label.items() if lbl == "Task-1"]
    if roots:
        return roots
    all_children = {c for kids in children.values() for c in kids}
    return [n for n in id2label if n not in all_children]


# ─── PRINT TREE FUNCTION ───────────────────────────────────────
def print_async_tree(result, task_emoji="(T)", cor_emoji="", printer=print):
    """
    Pretty-print the async call tree produced by `get_all_async_stacks()`,
    prefixing tasks with *task_emoji* and coroutine frames with *cor_emoji*.
    """
    id2name, awaits = _index(result)
    labels, children = _build_tree(id2name, awaits)

    def pretty(node):
        flag = task_emoji if node[0] == NodeType.TASK else cor_emoji
        return f"{flag} {labels[node]}"

    def render(node, prefix="", last=True, buf=None):
        if buf is None:
            buf = []
        buf.append(f"{prefix}{'└── ' if last else '├── '}{pretty(node)}")
        new_pref = prefix + ("    " if last else "│   ")
        kids = children.get(node, [])
        for i, kid in enumerate(kids):
            render(kid, new_pref, i == len(kids) - 1, buf)
        return buf

    result = []
    for r, root in enumerate(_roots(labels, children)):
        result.append(render(root))
    return result


# ─── BUILD TABLE FUNCTION ───────────────────────────────────────
def build_task_table(result):
    id2name, awaits = _index(result)
    table = []

    # Iterate through the result data
    for item in result:
        # Check the number of elements in `item`
        # Adjusting for possible structure variations
        if len(item) == 2:  # Case for two elements (likely thread id and list of tasks)
            tasks = item[1]
            for task_id, task_name, awaited in tasks:
                for stack, awaiter_id in awaited:
                    coroutine_chain = " -> ".join(stack)
                    awaiter_name = id2name.get(awaiter_id, "Unknown")
                    table.append([hex(task_id), task_name, coroutine_chain, awaiter_name, hex(awaiter_id)])
        else:
            # Handle any other structure (just in case we find something unexpected)
            print("DEBUG: Unexpected structure!")
            continue

    return table


# ─── MAIN FUNCTION ──────────────────────────────────────────────
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Show Python async tasks in a process (like 'ps' for Python tasks)")
    parser.add_argument("pid", type=int, help="Process ID(s) to inspect. If none specified, show all Python processes.")
    parser.add_argument("--tree", "-t", action="store_true", help="Display tasks in a tree format")
    args = parser.parse_args()
    tasks = get_all_awaited_by(args.pid)

    if args.tree:
        # Print the async call tree
        result = print_async_tree(tasks)
        for tree in result:
            print('\n'.join(tree))
    else:
        # Build and print the task table
        table = build_task_table(tasks)
        # Print the table in a simple tabular format
        print(f"{'task id':<10} {'task name':<20} {'coroutine chain':<50} {'awaiter name':<20} {'awaiter id':<15}")
        print("-" * 114)
        for row in table:
            print(f"{row[0]:<10} {row[1]:<20} {row[2]:<50} {row[3]:<20} {row[4]:<15}")

