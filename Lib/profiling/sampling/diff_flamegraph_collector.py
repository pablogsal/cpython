"""
Differential flamegraph collector for comparing two profiles.

This module provides the DiffFlamegraphCollector class which generates
differential flame graphs that show the difference between a baseline
profile and the current profile using red/blue coloring:
- Red: Function time increased or new function
- Blue: Function time decreased
- Frame widths: Based on the "after" (current) profile

Based on Brendan Gregg's differential flame graph technique:
https://www.brendangregg.com/blog/2014-11-09/differential-flame-graphs.html
"""

import json
import os
from .stack_collector import FlamegraphCollector
from .binary_reader import BinaryReader


def _get_func_key(node, string_table=None):
    """Generate a function-level key for matching between profiles.

    Uses just the function name for matching, allowing comparisons between:
    - The same script profiled before/after changes
    - Different scripts with the same function names (test baseline vs current)

    This is intentionally permissive - functions with the same name are
    assumed to be the same logical function for diff purposes.

    Args:
        node: A flamegraph node dict with filename, funcname
        string_table: Optional string table for resolving string indices

    Returns:
        The funcname as the key (simple string matching by function name)
    """
    funcname = node.get("funcname", "")

    # Resolve string indices if we have a string table
    if string_table:
        if isinstance(funcname, int) and 0 <= funcname < len(string_table):
            funcname = string_table[funcname]

    return funcname


def _sum_samples_per_function(tree, string_table=None):
    """Sum total samples for each unique function across all paths.

    Uses (filename, funcname) as key WITHOUT line numbers so that the same
    function at different lines is aggregated together. This ensures proper
    matching between baseline and current profiles.

    Args:
        tree: Root node of the flamegraph tree
        string_table: Optional string table for resolving string indices

    Returns:
        Dict mapping (filename, funcname) to total "self" samples
    """
    func_samples = {}

    def traverse(node):
        key = _get_func_key(node, string_table)

        # Calculate self time (guard against negative values from corrupt data)
        node_value = node.get("value", 0)
        children_value = sum(c.get("value", 0) for c in node.get("children", []))
        self_value = max(0, node_value - children_value)

        func_samples[key] = func_samples.get(key, 0) + self_value

        for child in node.get("children", []):
            traverse(child)

    traverse(tree)
    return func_samples


def compute_function_deltas(baseline_tree, current_tree, baseline_strings=None,
                            current_strings=None, normalize=True):
    """Compute per-function deltas (total change for each function).

    This computes the TOTAL delta for each function across ALL call paths,
    ensuring consistent delta values regardless of view orientation
    (normal vs inverted).

    Args:
        baseline_tree: Root node of baseline flamegraph
        current_tree: Root node of current flamegraph
        baseline_strings: String table for baseline tree
        current_strings: String table for current tree
        normalize: If True, normalize baseline to match current total

    Returns:
        Dict mapping (filename, funcname) to {delta, delta_pct, is_new}
    """
    baseline_total = baseline_tree.get("value", 0)
    current_total = current_tree.get("value", 0)

    # Calculate normalization scale factor
    scale = 1.0
    if normalize and baseline_total > 0 and current_total > 0:
        scale = current_total / baseline_total

    # Sum samples per function in both trees
    baseline_funcs = _sum_samples_per_function(baseline_tree, baseline_strings)
    current_funcs = _sum_samples_per_function(current_tree, current_strings)

    # Compute deltas
    function_deltas = {}
    all_funcs = baseline_funcs.keys() | current_funcs.keys()
    max_delta = 0

    for func_key in all_funcs:
        baseline_val = baseline_funcs.get(func_key, 0) * scale
        current_val = current_funcs.get(func_key, 0)
        delta = current_val - baseline_val
        delta_pct = (delta / current_total * 100) if current_total > 0 else 0
        is_new = func_key not in baseline_funcs

        # func_key is now just the function name (string)
        function_deltas[func_key] = {
            "delta": delta,
            "delta_pct": delta_pct,
            "is_new": is_new,
        }
        max_delta = max(max_delta, abs(delta))

    return {"deltas": function_deltas, "max_delta": max_delta}


def _build_aggregated_node_map(tree, string_table=None):
    """Build a map of path -> aggregated node data for a flamegraph tree.

    Uses function name only for path keys. When multiple siblings have the
    same function name, their self-times are aggregated together. This makes
    matching robust across different profile runs where sibling order may vary.

    Args:
        tree: Root node of the flamegraph tree
        string_table: Optional string table for resolving string indices

    Returns:
        Dict mapping path tuples to aggregated node data:
        {path: {"value": total_value, "self": total_self}}
    """
    node_map = {}

    def traverse(node, path):
        # Use function name only for cross-file matching
        funcname = _get_func_key(node, string_table)
        full_path = path + (funcname,)

        # Calculate self-time for this node (guard against negative values from corrupt data)
        node_value = node.get("value", 0)
        children_value = sum(c.get("value", 0) for c in node.get("children", []))
        node_self = max(0, node_value - children_value)

        # Aggregate into the map
        if full_path not in node_map:
            node_map[full_path] = {"value": 0, "self": 0}
        node_map[full_path]["value"] += node_value
        node_map[full_path]["self"] += node_self

        # Recurse into children
        for child in node.get("children", []):
            traverse(child, full_path)

    traverse(tree, ())
    return node_map


def _invert_tree(tree, string_table=None):
    """Invert a flamegraph tree (flip from root-to-leaf to leaf-to-root).

    In the inverted tree:
    - First level children are the original leaf functions (hotspots)
    - Their children are their callers
    - Values are aggregated for functions appearing in multiple paths

    Args:
        tree: Root node of the flamegraph tree
        string_table: Optional string table for resolving string indices

    Returns:
        A new inverted tree with the same structure
    """
    def resolve_str(val):
        """Resolve string index if we have a string table."""
        if string_table and isinstance(val, int) and 0 <= val < len(string_table):
            return string_table[val]
        return val

    inverted_root = {
        "name": resolve_str(tree.get("name")),
        "value": tree.get("value", 0),
        "children": {},  # Dict for deduplication during build
        "threads": tree.get("threads", []),
        "filename": resolve_str(tree.get("filename", "")),
        "lineno": tree.get("lineno", 0),
        "funcname": resolve_str(tree.get("funcname", "")),
    }

    def get_key(node):
        """Get unique key for a node."""
        return (resolve_str(node.get("filename", "")),
                node.get("lineno", 0),
                resolve_str(node.get("funcname", "")))

    def add_inverted_path(path, leaf_node):
        """Add a reversed path to the inverted tree."""
        current = inverted_root
        leaf_value = leaf_node.get("value", 0)

        # Process path from leaf to root (reversed)
        for node in reversed(path):
            key = get_key(node)
            if key not in current["children"]:
                current["children"][key] = {
                    "filename": resolve_str(node.get("filename", "")),
                    "lineno": node.get("lineno", 0),
                    "funcname": resolve_str(node.get("funcname", "")),
                    "name": resolve_str(node.get("name", "")),
                    "value": 0,
                    "children": {},
                    "threads": set() if isinstance(node.get("threads"), list) else node.get("threads", set()).copy(),
                }
            child = current["children"][key]
            child["value"] += leaf_value
            # Track threads
            threads = leaf_node.get("threads", [])
            if isinstance(threads, list):
                if isinstance(child["threads"], set):
                    child["threads"].update(threads)
                else:
                    child["threads"] = set(threads)
            current = child

    def traverse_and_collect(path, current_node):
        """Recursively traverse to find all leaf paths."""
        children = current_node.get("children", [])

        if not children:
            # Leaf node - process the full path
            add_inverted_path(path, current_node)
        else:
            for child in children:
                traverse_and_collect(path + [child], child)

    # Start traversal from each root child
    for child in tree.get("children", []):
        traverse_and_collect([child], child)

    def convert_to_lists(node):
        """Recursively convert children dicts to sorted lists."""
        if isinstance(node.get("children"), dict):
            children_list = list(node["children"].values())
            children_list.sort(key=lambda x: (-x.get("value", 0), str(x.get("name", ""))))
            node["children"] = children_list

        if isinstance(node.get("threads"), set):
            node["threads"] = sorted(node["threads"])

        for child in node.get("children", []):
            convert_to_lists(child)

    convert_to_lists(inverted_root)
    return inverted_root


def _build_elided_tree(baseline_tree, current_map, baseline_strings=None,
                       min_pct=0.5):
    """Build a flamegraph tree containing only elided stacks.

    An elided stack is a COMPLETE stack (from root to leaf) in baseline that
    does NOT exist in current at all. We only check LEAF nodes - if a leaf's
    full path doesn't exist in current, that entire stack is elided.

    Args:
        baseline_tree: Root node of baseline flamegraph
        current_map: Path map from current tree (to check what exists)
        baseline_strings: String table for baseline tree
        min_pct: Minimum percentage of baseline total to include (default 0.5%)

    Returns:
        A new flamegraph tree containing only elided stacks
    """
    baseline_total = baseline_tree.get("value", 0)
    min_value = (min_pct / 100.0) * baseline_total if baseline_total > 0 else 0

    def resolve_str(val):
        """Resolve string index if we have a string table."""
        if baseline_strings and isinstance(val, int) and 0 <= val < len(baseline_strings):
            return baseline_strings[val]
        return val

    elided_root = {
        "name": "all (elided)",
        "value": 0,
        "children": {},  # Use dict for dedup during build, convert to list at end
        "filename": "",
        "lineno": 0,
        "funcname": "all (elided)",
        "is_elided_root": True,
    }

    def get_node_key(node):
        """Get a unique key for a node."""
        return (
            resolve_str(node.get("filename", "")),
            node.get("lineno", 0),
            resolve_str(node.get("funcname", ""))
        )

    def add_elided_stack(stack, leaf_value):
        """Add an elided stack (full path from root to leaf) to the elided tree.

        Args:
            stack: List of nodes from root to leaf (the full call stack)
            leaf_value: The value at the leaf node
        """
        if not stack or leaf_value <= 0:
            return

        current = elided_root
        current["value"] += leaf_value

        # Walk through the stack and build/update nodes
        for node in stack:
            key = get_node_key(node)
            if key not in current["children"]:
                current["children"][key] = {
                    "name": resolve_str(node.get("name", "")),
                    "value": 0,
                    "children": {},
                    "filename": resolve_str(node.get("filename", "")),
                    "lineno": node.get("lineno", 0),
                    "funcname": resolve_str(node.get("funcname", "")),
                    "is_elided": True,
                }
            child = current["children"][key]
            child["value"] += leaf_value
            current = child

    def traverse_to_leaves(node, path, stack):
        """Recursively traverse to find LEAF nodes and check if their stacks are elided.

        Only leaf nodes are checked - if a leaf's path doesn't exist in current,
        the entire stack leading to that leaf is elided.
        """
        funcname = _get_func_key(node, baseline_strings)
        full_path = path + (funcname,)
        full_stack = stack + [node]

        children = node.get("children", [])

        if not children:
            # LEAF NODE - check if this exact stack exists in current
            # A stack is elided if its path has NO entry in current_map
            if full_path not in current_map:
                leaf_value = node.get("value", 0)
                # Only include if above minimum threshold (0.5% of baseline)
                if leaf_value >= min_value:
                    add_elided_stack(full_stack, leaf_value)
        else:
            # Not a leaf - recurse into children
            for child in children:
                traverse_to_leaves(child, full_path, full_stack)

    # Start from root's children, but include root funcname in path prefix
    # This ensures paths match current_map which includes the root "all"
    root_funcname = _get_func_key(baseline_tree, baseline_strings)
    for child in baseline_tree.get("children", []):
        traverse_to_leaves(child, (root_funcname,), [])

    def convert_to_lists(node):
        """Convert children dicts to sorted lists recursively."""
        if isinstance(node.get("children"), dict):
            children_list = list(node["children"].values())
            children_list.sort(key=lambda x: (-x.get("value", 0), str(x.get("name", ""))))
            node["children"] = children_list

        for child in node.get("children", []):
            convert_to_lists(child)

    convert_to_lists(elided_root)
    return elided_root


def compute_tree_diff(baseline_tree, current_tree, baseline_strings=None,
                      current_strings=None, normalize=True):
    """
    Compute differential between two flamegraph trees.

    Args:
        baseline_tree: Root node of baseline flamegraph
        current_tree: Root node of current flamegraph
        baseline_strings: String table for baseline tree
        current_strings: String table for current tree
        normalize: If True, normalize baseline to match current total

    Returns:
        Modified current_tree with delta values for each node:
        - delta: normalized difference (current - baseline)
        - delta_pct: percentage change relative to root value
        - is_new: True if function not in baseline
    """
    baseline_total = baseline_tree.get("value", 0)
    current_total = current_tree.get("value", 0)

    # Calculate normalization scale factor
    scale = 1.0
    if normalize and baseline_total > 0 and current_total > 0:
        scale = current_total / baseline_total

    # Build aggregated maps for both trees - aggregates self-time for siblings with same name
    baseline_map = _build_aggregated_node_map(baseline_tree, baseline_strings)
    current_map = _build_aggregated_node_map(current_tree, current_strings)

    # Calculate elided percentage: fraction of baseline self-time not present in current
    # Uses RAW baseline values (not normalized) since we want to know what % of
    # the original baseline work is no longer present in the current profile.
    # Based on SELF-time (exclusive) to avoid double-counting parent/child time.
    elided_self_time = 0
    for path, baseline_data in baseline_map.items():
        if path not in current_map:
            elided_self_time += baseline_data["self"]

    elided_pct = (elided_self_time / baseline_total * 100) if baseline_total > 0 else 0

    # Pre-compute deltas for each path (aggregated current vs aggregated baseline)
    path_deltas = {}
    max_delta = 0

    for path, current_data in current_map.items():
        current_self = current_data["self"]
        current_value = current_data["value"]
        baseline_data = baseline_map.get(path)

        if baseline_data is None:
            # New path not in baseline
            delta = current_self
            baseline_self_scaled = 0
            baseline_value_scaled = 0
            is_new = True
        else:
            baseline_self_scaled = baseline_data["self"] * scale
            baseline_value_scaled = baseline_data["value"] * scale
            delta = current_self - baseline_self_scaled
            is_new = False

        delta_pct = (delta / current_total * 100) if current_total > 0 else 0
        path_deltas[path] = {
            "delta": delta,
            "delta_pct": delta_pct,
            "is_new": is_new,
            "current_self": current_self,
            "baseline_self": baseline_self_scaled,
            "current_value": current_value,
            "baseline_value": baseline_value_scaled,
        }
        max_delta = max(max_delta, abs(delta))

    def annotate_node(node, path, current_strings):
        """Recursively annotate nodes with pre-computed path delta.

        Uses aggregated SELF time (exclusive) for delta calculation.
        All nodes at the same path get the same delta (the path's aggregate delta).
        """
        # Use function-name-only key for matching (cross-file compatibility)
        funcname = _get_func_key(node, current_strings)
        full_path = path + (funcname,)

        # Look up pre-computed delta for this path
        path_data = path_deltas.get(full_path, {
            "delta": 0, "delta_pct": 0, "is_new": False,
            "current_self": 0, "baseline_self": 0,
            "current_value": 0, "baseline_value": 0
        })

        node["delta"] = path_data["delta"]
        node["delta_pct"] = path_data["delta_pct"]
        node["is_new"] = path_data["is_new"]
        node["current_self"] = path_data["current_self"]
        node["baseline_self"] = path_data["baseline_self"]
        node["baseline_value"] = path_data["baseline_value"]

        # Recurse into children
        for child in node.get("children", []):
            annotate_node(child, full_path, current_strings)

    # Annotate the tree starting from root
    annotate_node(current_tree, (), current_strings)

    # Add max_delta to the root for color scaling in JS
    current_tree["max_delta"] = max_delta
    current_tree["is_diff_mode"] = True

    # Store elided percentage in the tree for display in the UI
    # This represents the fraction of baseline self-time not present in current
    current_tree["elided_pct"] = elided_pct
    current_tree["elided_self_time"] = elided_self_time
    current_tree["baseline_total"] = baseline_total

    # Build the elided flamegraph tree if there are elided paths
    # This is a SEPARATE tree that can be toggled in the UI
    # Per Brendan Gregg: "If you click the elided text, it takes you to the elided flame graph"
    if elided_pct > 0:
        elided_tree = _build_elided_tree(baseline_tree, current_map, baseline_strings)
        current_tree["elided_tree"] = elided_tree
    else:
        current_tree["elided_tree"] = None

    return current_tree


def compute_inverted_tree_diff(baseline_tree, current_tree, baseline_strings=None,
                                current_strings=None, normalize=True):
    """
    Compute differential for inverted (caller) view.

    This inverts BOTH trees first, then computes the diff. This ensures
    consistent delta values - a function will show the same delta whether
    viewed in normal or inverted mode.

    Args:
        baseline_tree: Root node of baseline flamegraph
        current_tree: Root node of current flamegraph
        baseline_strings: String table for baseline tree
        current_strings: String table for current tree
        normalize: If True, normalize baseline to match current total

    Returns:
        Inverted tree with proper delta values computed by comparing
        inverted baseline to inverted current.
    """
    # First invert both trees (this resolves strings during inversion)
    inverted_baseline = _invert_tree(baseline_tree, baseline_strings)
    inverted_current = _invert_tree(current_tree, current_strings)

    # Now compute diff on the inverted trees
    # Note: strings are already resolved by _invert_tree, so no string tables needed
    return compute_tree_diff(inverted_baseline, inverted_current,
                             baseline_strings=None, current_strings=None,
                             normalize=normalize)


class DiffFlamegraphCollector(FlamegraphCollector):
    """Collector that generates differential flame graphs by comparing against a baseline."""

    def __init__(self, sample_interval_usec, baseline_path, *, skip_idle=False):
        """Initialize the diff flamegraph collector.

        Args:
            sample_interval_usec: Sampling interval in microseconds
            baseline_path: Path to baseline binary profile file
            skip_idle: Whether to skip idle samples
        """
        super().__init__(sample_interval_usec, skip_idle=skip_idle)
        self.baseline_path = baseline_path
        self._baseline_data = None
        self._baseline_strings = None

    def _load_baseline(self):
        """Load and process the baseline profile from binary file."""
        if self._baseline_data is not None:
            return

        if not os.path.exists(self.baseline_path):
            raise FileNotFoundError(f"Baseline profile not found: {self.baseline_path}")

        # Create a temporary collector to receive the baseline samples
        baseline_collector = FlamegraphCollector(
            self.sample_interval_usec,
            skip_idle=self.skip_idle
        )

        # Load and replay the baseline profile using context manager
        with BinaryReader(self.baseline_path) as reader:
            reader.replay_samples(baseline_collector)

        # Convert to flamegraph format
        self._baseline_data = baseline_collector._convert_to_flamegraph_format()
        self._baseline_strings = self._baseline_data.get("strings", [])

    def export(self, filename):
        """Export the differential flamegraph to an HTML file.

        Args:
            filename: Output HTML file path
        """
        # Load baseline profile
        self._load_baseline()

        # Get current profile data
        current_data = self._convert_to_flamegraph_format()
        current_strings = current_data.get("strings", [])

        total_samples = current_data.get("value", 0)
        num_functions = len(current_data.get("children", []))
        string_count = len(self._string_table)

        print(
            f"Diff flamegraph data: {num_functions} root functions, total samples: {total_samples}, "
            f"{string_count} unique strings"
        )

        if num_functions == 0:
            print(
                "Warning: No functions found in profiling data. Check if sampling captured any data."
            )
            return

        # Compute the normal (callee) diff
        diff_data = compute_tree_diff(
            self._baseline_data,
            current_data,
            baseline_strings=self._baseline_strings,
            current_strings=current_strings,
            normalize=True
        )

        # Compute per-function deltas for CONSISTENT colors across all views
        # This ensures the same function shows the same delta in normal and inverted
        func_deltas = compute_function_deltas(
            self._baseline_data,
            current_data,
            baseline_strings=self._baseline_strings,
            current_strings=current_strings,
            normalize=True
        )
        diff_data["function_deltas"] = func_deltas["deltas"]
        diff_data["func_max_delta"] = func_deltas["max_delta"]

        # Note: inverted diff view is hidden in the UI (too confusing),
        # so we skip computing inverted_diff_tree to save processing time

        # Generate HTML
        html_content = self._create_flamegraph_html(diff_data)

        with open(filename, "w", encoding="utf-8") as f:
            f.write(html_content)

        print(f"Differential flamegraph saved to: {filename}")
        print(f"  Baseline: {self.baseline_path}")
        print(f"  Max delta: {diff_data.get('max_delta', 0):.0f} samples")
        elided_pct = diff_data.get('elided_pct', 0)
        if elided_pct > 0:
            print(f"  Elided: {elided_pct:.1f}% of baseline (click badge in UI to view)")
