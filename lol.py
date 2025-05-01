# ---------------------------------------------------------------
# async_tree_printer.py
# ---------------------------------------------------------------
from collections import defaultdict
from itertools import count


def _index(result):
    """
    Build:
      • id2name : {task_id: task_name}
      • awaits  : list of (parent_id, stack, child_id)
    """
    id2name, awaits = {}, []

    for _thread_id, tasks in result:
        for tid, tname, awaited in tasks:
            id2name[tid] = tname
            for stack, parent_id in awaited:
                awaits.append((parent_id, stack, tid))

    return id2name, awaits


def _build_tree(id2name, awaits):
    """
    Create a pure tree:
      children[parent_key]  --> list(child_keys)
    Nodes are tuples: ('task', id)  or  ('cor', synthetic_id)
    Task nodes are unique by *id*; coroutine nodes are unique
    **under their parent** by *display-name*, so the common
    frames inside a TaskGroup (_aexit, __aexit__, …) are not
    duplicated.
    """
    id2label = {('task', tid): name for tid, name in id2name.items()}
    children   = defaultdict(list)
    cor_names  = defaultdict(dict)        # parent_key -> {name: node_key}
    cor_id_seq = count(1)

    def _touch(parent_key, label):
        """Return a (cor …) node under *parent_key* with *label*,
        re-using an existing one if the label is already present."""
        lookup = cor_names[parent_key]
        if label in lookup:
            return lookup[label]

        node_key = ('cor', f"c{next(cor_id_seq)}")
        id2label[node_key] = label
        children[parent_key].append(node_key)
        lookup[label] = node_key
        return node_key

    # ensure every task appears (even those that await nobody)
    for tid in id2name:
        children[('task', tid)]  # touch

    # lay down the parent ➜ …stack… ➜ child paths
    for parent_id, stack, child_id in awaits:
        parent_key = ('task', parent_id)
        cur = parent_key
        for frame in reversed(stack):          # outer-most → inner-most
            cur = _touch(cur, frame)
        child_key = ('task', child_id)
        if child_key not in children[cur]:
            children[cur].append(child_key)

    return id2label, children


def _find_roots(id2label, children):
    """Prefer 'Task-1'.  Otherwise: every node that is never a child."""
    roots = [node for node, label in id2label.items() if label == "Task-1"]
    if roots:
        return roots

    all_children = {c for kid_list in children.values() for c in kid_list}
    return [n for n in id2label if n not in all_children]


# ─────────────────────────  public helper  ──────────────────────────
def print_async_tree(result):
    """
    Pretty-print the full coroutine / task tree that the *result*
    returned by `get_all_async_stacks()` describes.
    """
    id2name, awaits = _index(result)
    labels, children = _build_tree(id2name, awaits)

    def _render(node, prefix="", last=True, out=None):
        if out is None:
            out = []
        connector = "└── " if last else "├── "
        out.append(f"{prefix}{connector}{labels[node]}")
        new_pref = prefix + ("    " if last else "│   ")
        kid_list = children.get(node, [])
        for i, kid in enumerate(kid_list):
            _render(kid, new_pref, i == len(kid_list) - 1, out)
        return out

    for r_idx, root in enumerate(_find_roots(labels, children)):
        print("\n".join(_render(root)))
        if r_idx != 0:                       # blank line if several roots
            print()


# ---------------------------------------------------------------
# Example – remove this block in production
# ---------------------------------------------------------------
if __name__ == "__main__":
    #  ← the sample structure you posted
    SAMPLE = [
        (935723,
         [
             (4399485024, 'Task-1', []),
             (4399612496, 'timer',
              [[['blech3', 'blech2', 'blech'], 4398815424],
               [['plech3', 'plech2', 'plech'], 4397791776],
               [['plech3', 'plech2', 'plech'], 4397983808],
               [['blech3', 'blech2', 'blech'], 4397791328]]),
             (4399612992, 'root1', [[['_aexit', '__aexit__', 'main'], 4399485024]]),
             (4398813024, 'root2', [[['_aexit', '__aexit__', 'main'], 4399485024]]),
             (4398815424, 'child1_1', [[['_aexit', '__aexit__', 'blocho_caller', 'bloch'], 4399612992]]),
             (4397983808, 'child2_1', [[['_aexit', '__aexit__', 'blocho_caller', 'bloch'], 4399612992]]),
             (4397791328, 'child1_2', [[['_aexit', '__aexit__', 'blocho_caller', 'bloch'], 4398813024]]),
             (4397791776, 'child2_2', [[['_aexit', '__aexit__', 'blocho_caller', 'bloch'], 4398813024]]),
         ]),
        (0, []),
    ]

    print_async_tree(SAMPLE)

