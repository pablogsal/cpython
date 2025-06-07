import collections
import marshal
import pstats
import time
import _remote_debugging
import argparse
import _colorize
from _colorize import ANSIColors
import functools


class SampleProfile:
    def __init__(self, pid, sample_interval_usec, all_threads):
        self.pid = pid
        self.sample_interval_usec = sample_interval_usec
        self.all_threads = all_threads
        self.unwinder = _remote_debugging.RemoteUnwinder(
            self.pid, all_threads=self.all_threads
        )
        self.stats = {}
        self.callers = collections.defaultdict(
            lambda: collections.defaultdict(int)
        )
        # NEW: Store actual call trees for flamegraph
        self.call_trees = []
        self.function_samples = collections.defaultdict(int)

    def sample(self, duration_sec=10):
        result = collections.defaultdict(
            lambda: dict(total_calls=0, total_rec_calls=0, inline_calls=0)
        )
        sample_interval_sec = self.sample_interval_usec / 1_000_000

        running_time = 0
        num_samples = 0
        errors = 0
        start_time = next_time = time.perf_counter()
        while running_time < duration_sec:
            if next_time < time.perf_counter():
                try:
                    stack_frames = self.unwinder.get_stack_trace()
                    self.aggregate_stack_frames(result, stack_frames)
                    # NEW: Store the actual stack traces for flamegraph
                    self.store_call_trees(stack_frames)
                except (RuntimeError, UnicodeDecodeError, OSError):
                    errors += 1

                num_samples += 1
                next_time += sample_interval_sec

            running_time = time.perf_counter() - start_time

        print(f"Captured {num_samples} samples in {running_time:.2f} seconds")
        print(f"Sample rate: {num_samples / running_time:.2f} samples/sec")
        print(f"Error rate: {(errors / num_samples) * 100:.2f}%")

        expected_samples = int(duration_sec / sample_interval_sec)
        if num_samples < expected_samples:
            print(
                f"Warning: missed {expected_samples - num_samples} samples "
                f"from the expected total of {expected_samples} "
                f"({(expected_samples - num_samples) / expected_samples * 100:.2f}%)"
            )

        self.stats = self.convert_to_pstats(result)

    def store_call_trees(self, stack_frames):
        """Store call trees from stack traces for flamegraph generation"""
        for thread_id, frames in stack_frames:
            if frames and len(frames) > 0:
                # Store the complete call stack (reverse order - root first)
                call_tree = list(reversed(frames))
                self.call_trees.append(call_tree)
                
                # Count samples per function
                for frame in frames:
                    self.function_samples[frame] += 1

    def print_stats(self, sort=-1, limit=None, show_summary=True):
        if not isinstance(sort, tuple):
            sort = (sort,)
        stats = pstats.SampledStats(self).strip_dirs()

        # Get the stats data
        stats_list = []
        for func, (cc, nc, tt, ct, callers) in stats.stats.items():
            stats_list.append((func, cc, nc, tt, ct, callers))

        # Sort based on the requested field
        sort_field = sort[0]
        if sort_field == -1:  # stdname
            stats_list.sort(key=lambda x: str(x[0]))
        elif sort_field == 0:  # calls
            stats_list.sort(key=lambda x: x[2], reverse=True)
        elif sort_field == 1:  # time
            stats_list.sort(key=lambda x: x[3], reverse=True)
        elif sort_field == 2:  # cumulative
            stats_list.sort(key=lambda x: x[4], reverse=True)
        elif sort_field == 3:  # percall
            stats_list.sort(
                key=lambda x: x[3] / x[2] if x[2] > 0 else 0, reverse=True
            )
        elif sort_field == 4:  # cumpercall
            stats_list.sort(
                key=lambda x: x[4] / x[2] if x[2] > 0 else 0, reverse=True
            )
        elif sort_field == 5:  # name (alphabetical)
            stats_list.sort(key=lambda x: str(x[0]))

        # Apply limit if specified
        if limit is not None:
            stats_list = stats_list[:limit]

        # Find the maximum values for each column to determine units
        max_tt = max((tt for _, _, _, tt, _, _ in stats_list), default=0)
        max_ct = max((ct for _, _, _, _, ct, _ in stats_list), default=0)

        # Determine appropriate units and format strings
        if max_tt >= 1.0:
            tt_unit = "s"
            tt_scale = 1.0
        elif max_tt >= 0.001:
            tt_unit = "ms"
            tt_scale = 1000.0
        else:
            tt_unit = "μs"
            tt_scale = 1000000.0

        if max_ct >= 1.0:
            ct_unit = "s"
            ct_scale = 1.0
        elif max_ct >= 0.001:
            ct_unit = "ms"
            ct_scale = 1000.0
        else:
            ct_unit = "μs"
            ct_scale = 1000000.0

        # Print header with colors and units
        header = (
            f"{ANSIColors.BOLD_BLUE}Profile Stats:{ANSIColors.RESET}\n"
            f"{ANSIColors.BOLD_BLUE}ncalls{ANSIColors.RESET}      "
            f"{ANSIColors.BOLD_BLUE}tottime ({tt_unit}){ANSIColors.RESET}  "
            f"{ANSIColors.BOLD_BLUE}percall ({tt_unit}){ANSIColors.RESET}  "
            f"{ANSIColors.BOLD_BLUE}cumtime ({ct_unit}){ANSIColors.RESET}  "
            f"{ANSIColors.BOLD_BLUE}percall ({ct_unit}){ANSIColors.RESET}  "
            f"{ANSIColors.BOLD_BLUE}filename:lineno(function){ANSIColors.RESET}"
        )
        print(header)

        # Print each line with colors
        for func, cc, nc, tt, ct, callers in stats_list:
            if nc != cc:
                ncalls = f"{nc}/{cc}"
            else:
                ncalls = str(nc)

            # Format numbers with proper alignment and precision (no colors)
            tottime = f"{tt * tt_scale:8.3f}"
            percall = f"{(tt / nc) * tt_scale:8.3f}" if nc > 0 else "    N/A"
            cumtime = f"{ct * ct_scale:8.3f}"
            cumpercall = (
                f"{(ct / nc) * ct_scale:8.3f}" if nc > 0 else "    N/A"
            )

            # Format the function name with colors
            func_name = (
                f"{ANSIColors.GREEN}{func[0]}{ANSIColors.RESET}:"
                f"{ANSIColors.YELLOW}{func[1]}{ANSIColors.RESET}("
                f"{ANSIColors.CYAN}{func[2]}{ANSIColors.RESET})"
            )

            # Print the formatted line
            print(
                f"{ncalls:>8}  {tottime}  {percall}  "
                f"{cumtime}  {cumpercall}  {func_name}"
            )

        def _format_func_name(func):
            """Format function name with colors."""
            return (
                f"{ANSIColors.GREEN}{func[0]}{ANSIColors.RESET}:"
                f"{ANSIColors.YELLOW}{func[1]}{ANSIColors.RESET}("
                f"{ANSIColors.CYAN}{func[2]}{ANSIColors.RESET})"
            )

        def _print_top_functions(
            stats_list, title, key_func, format_line, n=3
        ):
            """Print top N functions sorted by key_func with formatted output."""
            print(f"\n{ANSIColors.BOLD_BLUE}{title}:{ANSIColors.RESET}")
            sorted_stats = sorted(stats_list, key=key_func, reverse=True)
            for stat in sorted_stats[:n]:
                if line := format_line(stat):
                    print(f"  {line}")

        # Print summary of interesting functions if enabled
        if show_summary and stats_list:
            print(
                f"\n{ANSIColors.BOLD_BLUE}Summary of Interesting Functions:{ANSIColors.RESET}"
            )

            # Most time-consuming functions (by total time)
            def format_time_consuming(stat):
                func, _, nc, tt, _, _ = stat
                if tt > 0:
                    return (
                        f"{tt * tt_scale:8.3f} {tt_unit} total time, "
                        f"{(tt / nc) * tt_scale:8.3f} {tt_unit} per call: {_format_func_name(func)}"
                    )
                return None

            _print_top_functions(
                stats_list,
                "Most Time-Consuming Functions",
                key_func=lambda x: x[3],
                format_line=format_time_consuming,
            )

            # Most called functions
            def format_most_called(stat):
                func, _, nc, tt, _, _ = stat
                if nc > 0:
                    return (
                        f"{nc:8d} calls, {(tt / nc) * tt_scale:8.3f} {tt_unit} "
                        f"per call: {_format_func_name(func)}"
                    )
                return None

            _print_top_functions(
                stats_list,
                "Most Called Functions",
                key_func=lambda x: x[2],
                format_line=format_most_called,
            )

            # Functions with highest per-call overhead
            def format_overhead(stat):
                func, _, nc, tt, _, _ = stat
                if nc > 0 and tt > 0:
                    return (
                        f"{(tt / nc) * tt_scale:8.3f} {tt_unit} per call, "
                        f"{nc:8d} calls: {_format_func_name(func)}"
                    )
                return None

            _print_top_functions(
                stats_list,
                "Functions with Highest Per-Call Overhead",
                key_func=lambda x: x[3] / x[2] if x[2] > 0 else 0,
                format_line=format_overhead,
            )

            # Functions with highest cumulative impact
            def format_cumulative(stat):
                func, _, nc, _, ct, _ = stat
                if ct > 0:
                    return (
                        f"{ct * ct_scale:8.3f} {ct_unit} cumulative time, "
                        f"{(ct / nc) * ct_scale:8.3f} {ct_unit} per call: "
                        f"{_format_func_name(func)}"
                    )
                return None

            _print_top_functions(
                stats_list,
                "Functions with Highest Cumulative Impact",
                key_func=lambda x: x[4],
                format_line=format_cumulative,
            )

    def dump_stats(self, file):
        stats_with_marker = dict(self.stats)
        stats_with_marker[("__sampled__",)] = True
        with open(file, "wb") as f:
            marshal.dump(stats_with_marker, f)

    def generate_flamegraph(self, output_path):
        """Generate a beautiful Python-branded flamegraph HTML file"""
        flamegraph_data = self._convert_to_flamegraph_format()
        
        # Debug output
        num_functions = len(flamegraph_data.get("children", []))
        total_time = flamegraph_data.get("value", 0)
        print(f"Flamegraph data: {num_functions} root functions, total samples: {total_time}")
        
        if num_functions == 0:
            print("Warning: No functions found in profiling data. Check if sampling captured any data.")
            return
        
        html_content = self._create_flamegraph_html(flamegraph_data)
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(html_content)
        
        print(f"Flamegraph saved to: {output_path}")

    @functools.lru_cache(maxsize=None)
    def _format_function_name(self, func):
        """Format function name for display in flamegraph - now cached!"""
        filename, lineno, funcname = func
        
        # Shorten long file paths
        if len(filename) > 50:
            parts = filename.split('/')
            if len(parts) > 2:
                filename = f".../{'/'.join(parts[-2:])}"
        
        return f"{funcname} ({filename}:{lineno})"

    def _convert_to_flamegraph_format(self):
        """Convert call trees to d3-flamegraph format with optimized hierarchy building"""
        if not self.call_trees:
            return {"name": "No Data", "value": 0, "children": []}
        
        unique_functions = set()
        for call_tree in self.call_trees:
            unique_functions.update(call_tree)
        
        # Create a mapping from function tuples to their formatted names
        func_to_name = {func: self._format_function_name(func) for func in unique_functions}
        
        # Build tree structure from all call stacks (following original algorithm exactly)
        root = {"name": "root", "children": {}, "samples": 0}
        
        for call_tree in self.call_trees:
            current_node = root
            current_node["samples"] += 1
            
            # Walk down the call tree (from root to leaf) using pre-computed names
            for func in call_tree:
                func_name = func_to_name[func]  # Use pre-computed name (major speedup!)
                
                if func_name not in current_node["children"]:
                    current_node["children"][func_name] = {
                        "name": func_name,
                        "func": func,
                        "children": {},
                        "samples": 0,
                        "filename": func[0],
                        "lineno": func[1], 
                        "funcname": func[2]
                    }
                
                current_node = current_node["children"][func_name]
                current_node["samples"] += 1
        
        def convert_node(node, min_samples=1):
            if node["samples"] < min_samples:
                return None
                
            # Get source code for this function if it's not the root
            source_code = None
            if "func" in node:
                source_code = self._get_source_lines(node["func"])
            
            result = {
                "name": node["name"],
                "value": node["samples"],  # Each sample represents time
                "children": []
            }
            
            # Add extra metadata if available
            if "filename" in node:
                result.update({
                    "filename": node["filename"],
                    "lineno": node["lineno"],
                    "funcname": node["funcname"]
                })
            
            if source_code:
                result["source"] = source_code
            
            # Recursively convert children
            child_nodes = []
            for child_name, child_node in node["children"].items():
                child_result = convert_node(child_node, min_samples)
                if child_result:
                    child_nodes.append(child_result)
            
            # Sort children by sample count (descending)
            child_nodes.sort(key=lambda x: x["value"], reverse=True)
            result["children"] = child_nodes
            
            return result
        
        # Filter out very small functions (less than 0.1% of total samples)
        total_samples = len(self.call_trees)
        min_samples = max(1, int(total_samples * 0.001))  # 0.1% threshold
        
        converted_root = convert_node(root, min_samples)
        
        if not converted_root or not converted_root["children"]:
            return {"name": "No significant data", "value": 0, "children": []}
        
        # If we only have one root child, make it the root to avoid redundant level
        if len(converted_root["children"]) == 1:
            main_child = converted_root["children"][0]
            main_child["name"] = f"Program Root: {main_child['name']}"
            return main_child
        
        converted_root["name"] = "Program Root"
        return converted_root
    
    def _get_source_lines(self, func):
        """Get source code lines for a function using linecache"""
        import linecache
        
        filename, lineno, funcname = func
        
        try:
            # Get several lines around the function definition
            lines = []
            start_line = max(1, lineno - 2)
            end_line = lineno + 3
            
            for line_num in range(start_line, end_line):
                line = linecache.getline(filename, line_num)
                if line.strip():  # Only include non-empty lines
                    # Mark the actual function line
                    marker = "→ " if line_num == lineno else "  "
                    lines.append(f"{marker}{line_num}: {line.rstrip()}")
            
            return lines if lines else None
            
        except Exception:
            # If we can't get source code, return None
            return None

    def _create_flamegraph_html(self, data):
        """Create a beautiful Python-branded HTML template for the flamegraph"""
        import json
        import os
        
        data_json = json.dumps(data)
        
        # Get the template file path relative to this module
        template_dir = os.path.dirname(__file__)
        template_path = os.path.join(template_dir, 'flamegraph_template.html')
        css_path = os.path.join(template_dir, 'flamegraph.css')
        js_path = os.path.join(template_dir, 'flamegraph.js')
        
        try:
            # Read all files
            with open(template_path, 'r', encoding='utf-8') as f:
                html_template = f.read()
            with open(css_path, 'r', encoding='utf-8') as f:
                css_content = f.read()
            with open(js_path, 'r', encoding='utf-8') as f:
                js_content = f.read()
                
            # Replace the placeholders with actual content
            html_template = html_template.replace(
                '<!-- INLINE_CSS -->',
                f'<style>\n{css_content}\n</style>'
            )
            html_template = html_template.replace(
                '<!-- INLINE_JS -->',
                f'<script>\n{js_content}\n</script>'
            )
            
            # Replace the placeholder with actual data
            html_content = html_template.replace('{{FLAMEGRAPH_DATA}}', data_json)
            
            return html_content
            
        except FileNotFoundError as e:
            # Fallback to a minimal template if any file is not found
            return f'''<!DOCTYPE html>
<html><head><title>Flamegraph Error</title></head>
<body><h1>Error: Required files not found</h1>
<p>Could not find required files: {str(e)}</p>
<p>Required files:</p>
<ul>
    <li>flamegraph_template.html</li>
    <li>flamegraph.css</li>
    <li>flamegraph.js</li>
</ul>
</body></html>'''

    # Needed for compatibility with pstats.Stats
    def create_stats(self):
        pass

    def convert_to_pstats(self, raw_results):
        sample_interval_sec = self.sample_interval_usec / 1_000_000
        pstats = {}
        callers = {}
        for fname, call_counts in raw_results.items():
            total = call_counts["inline_calls"] * sample_interval_sec
            cumulative = call_counts["total_calls"] * sample_interval_sec
            callers = dict(self.callers.get(fname, {}))
            pstats[fname] = (
                call_counts["total_calls"],
                call_counts["total_rec_calls"]
                if call_counts["total_rec_calls"]
                else call_counts["total_calls"],
                total,
                cumulative,
                callers,
            )
        return pstats

    def aggregate_stack_frames(self, result, stack_frames):
        for thread_id, frames in stack_frames:
            if not frames:
                continue
            top_location = frames[0]
            result[top_location]["inline_calls"] += 1
            result[top_location]["total_calls"] += 1

            for i in range(1, len(frames)):
                callee = frames[i - 1]
                caller = frames[i]
                self.callers[callee][caller] += 1

            if len(frames) <= 1:
                continue

            for location in frames[1:]:
                result[location]["total_calls"] += 1
                if top_location == location:
                    result[location]["total_rec_calls"] += 1


def sample(
    pid,
    *,
    sort=-1,
    sample_interval_usec=100,
    duration_sec=10,
    filename=None,
    all_threads=False,
    limit=None,
    show_summary=True,
    flamegraph=None,
):
    profile = SampleProfile(pid, sample_interval_usec, all_threads=all_threads)
    profile.sample(duration_sec)
    if filename:
        profile.dump_stats(filename)
    else:
        profile.print_stats(sort, limit, show_summary)

    if flamegraph:
        profile.generate_flamegraph(flamegraph)


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Sample a process's stack frames.\n\n"
            "Sort options:\n"
            "  --sort-calls      Sort by number of calls (most called functions first)\n"
            "  --sort-time       Sort by total time (most time-consuming functions first)\n"
            "  --sort-cumulative Sort by cumulative time (functions with highest total impact first)\n"
            "  --sort-percall    Sort by time per call (functions with highest per-call overhead first)\n"
            "  --sort-cumpercall Sort by cumulative time per call (functions with highest cumulative overhead per call)\n"
            "  --sort-name       Sort by function name (alphabetical order)\n\n"
            "The default sort is by cumulative time (--sort-cumulative).\n\n"
            "Flamegraph output:\n"
            "  --flamegraph FILE Generate an interactive HTML flamegraph visualization\n"
            "                    and save it to FILE. The flamegraph provides a beautiful,\n"
            "                    interactive way to explore function call performance with\n"
            "                    Python branding and zooming capabilities."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("pid", type=int, help="Process ID to sample.")
    parser.add_argument(
        "-i",
        "--interval",
        type=int,
        default=10,
        help="Sampling interval in microseconds (default: 10 usec)",
    )
    parser.add_argument(
        "-d",
        "--duration",
        type=int,
        default=10,
        help="Sampling duration in seconds (default: 10 seconds)",
    )
    parser.add_argument(
        "-a",
        "--all-threads",
        action="store_true",
        help="Sample all threads in the process",
    )
    parser.add_argument("-o", "--outfile", help="Save stats to <outfile>")
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="Disable color output",
    )
    parser.add_argument(
        "-l",
        "--limit",
        type=int,
        help="Limit the number of rows in the output",
    )
    parser.add_argument(
        "--no-summary",
        action="store_true",
        help="Disable the summary section at the end of the output",
    )
    parser.add_argument(
        "--flamegraph",
        help="Generate a flamegraph HTML file and save it to the specified path",
    )

    # Add sorting options
    sort_group = parser.add_mutually_exclusive_group()
    sort_group.add_argument(
        "--sort-calls",
        action="store_const",
        const=0,
        dest="sort",
        help="Sort by number of calls (most called functions first)",
    )
    sort_group.add_argument(
        "--sort-time",
        action="store_const",
        const=1,
        dest="sort",
        help="Sort by total time (most time-consuming functions first)",
    )
    sort_group.add_argument(
        "--sort-cumulative",
        action="store_const",
        const=2,
        dest="sort",
        help="Sort by cumulative time (functions with highest total impact first)",
    )
    sort_group.add_argument(
        "--sort-percall",
        action="store_const",
        const=3,
        dest="sort",
        help="Sort by time per call (functions with highest per-call overhead first)",
    )
    sort_group.add_argument(
        "--sort-cumpercall",
        action="store_const",
        const=4,
        dest="sort",
        help="Sort by cumulative time per call (functions with highest cumulative overhead per call)",
    )
    sort_group.add_argument(
        "--sort-name",
        action="store_const",
        const=5,
        dest="sort",
        help="Sort by function name (alphabetical order)",
    )

    # Set default sort to cumulative time
    parser.set_defaults(sort=2)

    args = parser.parse_args()

    # Set color theme based on --no-color flag
    if args.no_color:
        _colorize.set_theme(_colorize.theme_no_color)

    sample(
        args.pid,
        sample_interval_usec=args.interval,
        duration_sec=args.duration,
        filename=args.outfile,
        all_threads=args.all_threads,
        limit=args.limit,
        sort=args.sort,
        show_summary=not args.no_summary,
        flamegraph=args.flamegraph,
    )


if __name__ == "__main__":
    main()