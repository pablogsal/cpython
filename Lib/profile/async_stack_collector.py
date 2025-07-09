import collections
import os
from .stack_collectors import StackTraceCollector


class AsyncStackCollector(StackTraceCollector):
    def __init__(self, only_current_task=True):
        super().__init__()
        self.async_stacks = []
        self.only_current_task = only_current_task

    def collect(self, unwinder):
        # Now collect async task information
        if self.only_current_task:
            awaited_info = unwinder.get_async_stack_trace()
        else:
            awaited_info = unwinder.get_all_awaited_by()
        if awaited_info:
            self._process_async_stacks(awaited_info)
    
    def _process_async_stacks(self, awaited_info):
        """Process the async task information and reconstruct full stacks."""
        # Build a map of task_id to TaskInfo for easy lookup
        task_map = {}
        
        for thread_info in awaited_info:
            if thread_info.thread_id == 0:
                continue  # Skip empty thread info
                
            for task_info in thread_info.awaited_by:
                task_map[task_info.task_id] = task_info
        
        # For each task, reconstruct the full async stack
        for thread_info in awaited_info:
            if thread_info.thread_id == 0:
                continue
                
            for task_info in thread_info.awaited_by:
                full_stacks = self._reconstruct_task_stacks(task_info, task_map)
                self.async_stacks.extend(full_stacks)
    
    def _reconstruct_task_stacks(self, task_info, task_map, visited=None):
        """Recursively reconstruct full async stacks for a task."""
        if visited is None:
            visited = set()
            
        if task_info.task_id in visited:
            return []  # Avoid infinite recursion
            
        visited.add(task_info.task_id)
        full_stacks = []
        
        # Get the current task's stack from its coroutine_stack
        for coro_info in task_info.coroutine_stack:
            current_stack = []
            for frame in coro_info.call_stack:
                # Convert FrameInfo to tuple format (filename, lineno, funcname)
                current_stack.append((frame.filename, frame.lineno, frame.funcname))
            
            # If this task is awaited by others, we need to connect to awaiter stacks
            if task_info.awaited_by:
                for awaiter_coro in task_info.awaited_by:
                    # The task_name in CoroInfo is actually the task_id of the awaiting task
                    awaiter_task_id = awaiter_coro.task_name
                    
                    if awaiter_task_id in task_map:
                        awaiter_task = task_map[awaiter_task_id]
                        # Recursively get the awaiter's full stacks
                        awaiter_stacks = self._reconstruct_task_stacks(
                            awaiter_task, task_map, visited
                        )
                        if awaiter_stacks:
                            # For each awaiter stack, create a combined stack
                            for awaiter_stack in awaiter_stacks:
                                # Build: current_stack + [current_task_name] + awaiter_stack
                                full_stack = current_stack[:]
                                # Insert task name as transition between this task and its awaiter
                                if task_info.task_name and not task_info.task_name.startswith('Task-'):
                                    full_stack.append(("", 0, f"[Task:{task_info.task_name}]"))
                                full_stack.extend(awaiter_stack)
                                full_stacks.append(full_stack)
                        else:
                            # Awaiter has no further awaiters, build final stack
                            full_stack = current_stack[:]
                            # Add current task name
                            if task_info.task_name and not task_info.task_name.startswith('Task-'):
                                full_stack.append(("", 0, f"[Task:{task_info.task_name}]"))
                            # Add awaiter's coroutine stack
                            for awaiter_task_coro in awaiter_task.coroutine_stack:
                                for frame in awaiter_task_coro.call_stack:
                                    full_stack.append((frame.filename, frame.lineno, frame.funcname))
                            # Add awaiter task name at the end if it's a leaf
                            if awaiter_task.task_name and not awaiter_task.task_name.startswith('Task-'):
                                full_stack.append(("", 0, f"[Task:{awaiter_task.task_name}]"))
                            full_stacks.append(full_stack)
                    else:
                        # Can't find awaiter task in map, use the coroutine info directly
                        full_stack = current_stack[:]
                        # Add current task name
                        if task_info.task_name and not task_info.task_name.startswith('Task-'):
                            full_stack.append(("", 0, f"[Task:{task_info.task_name}]"))
                        # Add awaiter coroutine's stack
                        for frame in awaiter_coro.call_stack:
                            full_stack.append((frame.filename, frame.lineno, frame.funcname))
                        full_stacks.append(full_stack)
            else:
                # No awaiters, this is a leaf task - just add task name at the end
                full_stack = current_stack[:]
                if task_info.task_name and not task_info.task_name.startswith('Task-'):
                    full_stack.append(("", 0, f"[Task:{task_info.task_name}]"))
                full_stacks.append(full_stack)
        
        visited.remove(task_info.task_id)
        return full_stacks
    
    def export(self, filename):
        """Export both regular and async stacks in collapsed format."""
        stack_counter = collections.Counter()
        
        # Add regular stacks
        for call_tree in self.call_trees:
            stack_str = ";".join(
                f"{os.path.basename(f[0])}:{f[2]}:{f[1]}" for f in call_tree
            )
            stack_counter[stack_str] += 1
        
        # Add async stacks with [async] prefix
        for async_stack in self.async_stacks:
            # Reverse to get root->leaf order
            async_stack_reversed = list(reversed(async_stack))
            stack_parts = ["[async]"]
            for f in async_stack_reversed:
                if f[0] == "" and f[1] == 0 and f[2].startswith("[Task:"):
                    # This is a task name pseudo-frame
                    stack_parts.append(f[2])
                else:
                    # Regular frame
                    stack_parts.append(f"{os.path.basename(f[0])}:{f[2]}:{f[1]}")
            stack_str = ";".join(stack_parts)
            stack_counter[stack_str] += 1
        
        with open(filename, "w") as f:
            for stack, count in stack_counter.items():
                f.write(f"{stack} {count}\n")
        
        total_regular = len(self.call_trees)
        total_async = len(self.async_stacks)
        print(f"Collapsed stack output written to {filename}")
        print(f"  Regular stacks: {total_regular}")
        print(f"  Async stacks: {total_async}")