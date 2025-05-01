from deps import dependency_data

def print_task_dependency_tree(dependency_data):
    # Extract the tasks data
    tasks_data = dependency_data[0][1]

    # Build complete dependency graph
    awaiter_graph = {}  # Who awaits whom

    # Process all task entries to build the graph
    for task_entry in tasks_data:
        task_name = task_entry[0]
        dependencies = task_entry[1]

        # Create node if it doesn't exist
        if task_name not in awaiter_graph:
            awaiter_graph[task_name] = {
                'type': 'task',
                'awaits': [],      # Nodes this node awaits
                'awaited_by': []   # Nodes awaiting this node
            }

        # Process all dependency paths
        for path in dependencies:
            extract_dependencies(awaiter_graph, task_name, path)

    # Find bottom nodes (not awaiting anything)
    bottom_nodes = []
    for node, data in awaiter_graph.items():
        if not data['awaits'] and data['type'] == 'task':
            bottom_nodes.append(node)

    # Print header
    print("\nTask/Coroutine Dependency Tree (bottom → top)")
    print("===========================================")

    # Print tree for each bottom node
    for bottom in sorted(bottom_nodes):
        print_tree_bottom_up(awaiter_graph, bottom, "", True)

def extract_dependencies(graph, current_node, path):
    """Extract all dependencies from a path and update the graph"""
    if not path:
        return

    if isinstance(path[0], list):
        # It's a coroutine chain
        coroutine_chain = path[0]
        next_task = path[1]
        next_paths = path[2] if len(path) > 2 else []

        # Add all coroutines to the graph
        current = current_node

        # First, connect the task to the first coroutine
        if coroutine_chain:
            first_coroutine = coroutine_chain[-1]  # Reversed order for bottom-up

            # Make sure the coroutine exists in the graph
            if first_coroutine not in graph:
                graph[first_coroutine] = {
                    'type': 'coroutine',
                    'awaits': [],
                    'awaited_by': []
                }

            # Create the connection
            if first_coroutine not in graph[current]['awaits']:
                graph[current]['awaits'].append(first_coroutine)
            if current not in graph[first_coroutine]['awaited_by']:
                graph[first_coroutine]['awaited_by'].append(current)

            current = first_coroutine

        # Add all coroutines in reverse order (for bottom-up)
        for i in range(len(coroutine_chain) - 2, -1, -1):
            coroutine = coroutine_chain[i]

            # Make sure the coroutine exists in the graph
            if coroutine not in graph:
                graph[coroutine] = {
                    'type': 'coroutine',
                    'awaits': [],
                    'awaited_by': []
                }

            # Create the connection
            if coroutine not in graph[current]['awaits']:
                graph[current]['awaits'].append(coroutine)
            if current not in graph[coroutine]['awaited_by']:
                graph[coroutine]['awaited_by'].append(current)

            current = coroutine

        # Connect to the next task
        if next_task not in graph:
            graph[next_task] = {
                'type': 'task',
                'awaits': [],
                'awaited_by': []
            }

        if next_task not in graph[current]['awaits']:
            graph[current]['awaits'].append(next_task)
        if current not in graph[next_task]['awaited_by']:
            graph[next_task]['awaited_by'].append(current)

        # Process remaining paths
        for next_path in next_paths:
            extract_dependencies(graph, next_task, next_path)

    else:
        # Direct task dependency
        next_task = path

        # Make sure the task exists in the graph
        if next_task not in graph:
            graph[next_task] = {
                'type': 'task',
                'awaits': [],
                'awaited_by': []
            }

        # Create the connection
        if next_task not in graph[current_node]['awaits']:
            graph[current_node]['awaits'].append(next_task)
        if current_node not in graph[next_task]['awaited_by']:
            graph[next_task]['awaited_by'].append(current_node)

        # Process remaining paths
        if len(path) > 1 and isinstance(path[1], list):
            for next_path in path[1]:
                extract_dependencies(graph, next_task, next_path)

def print_tree_bottom_up(graph, node, prefix, is_last):
    """Print a tree from bottom to top"""
    # Determine node type and icon
    node_type = graph[node]['type']
    icon = "🔵" if node_type == 'task' else "⚙️"

    # Determine the branch characters
    branch = "└── " if is_last else "├── "

    # Print the current node
    print(f"{prefix}{branch}{icon} {node}")

    # Prepare the new prefix for awaited nodes
    new_prefix = prefix + ("    " if is_last else "│   ")

    # Sort awaits for consistent output
    awaits = sorted(graph[node]['awaits'])

    # Recursively print awaited nodes
    for i, awaited_node in enumerate(awaits):
        is_last_child = i == len(awaits) - 1
        print_tree_bottom_up(graph, awaited_node, new_prefix, is_last_child)


print_task_dependency_tree(dependency_data)
