from .pstats_collector import PstatsCollector


class AsyncPstatsCollector(PstatsCollector):
    def __init__(self, sample_interval_usec, only_current_task=True):
        super().__init__(sample_interval_usec)
        self.only_current_task = only_current_task
        self.async_sample_count = 0
        
    def collect(self, unwinder):
        if self.only_current_task:
            awaited_info = unwinder.get_async_stack_trace()
        else:
            awaited_info = unwinder.get_all_awaited_by()
        if awaited_info:
            self._process_async_stats(awaited_info)
    
    def _process_async_stats(self, awaited_info):
        """Process the async task information and update stats."""
        # Build a map of task_id to TaskInfo for easy lookup
        task_map = {}
        
        for thread_info in awaited_info:
            if thread_info.thread_id == 0:
                continue  # Skip empty thread info
                
            for task_info in thread_info.awaited_by:
                task_map[task_info.task_id] = task_info
        
        # For each task, reconstruct the full async stack and update stats
        for thread_info in awaited_info:
            if thread_info.thread_id == 0:
                continue
                
            for task_info in thread_info.awaited_by:
                full_stacks = self._reconstruct_task_stacks(task_info, task_map)
                for stack in full_stacks:
                    self._update_stats_from_stack(stack)
                    self.async_sample_count += 1
    
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
            
            # Add a pseudo-frame for the task name if it's not a generic name
            if task_info.task_name and not task_info.task_name.startswith('Task-'):
                # Add task name as a pseudo-frame
                current_stack.append(("<task>", 0, f"[Task:{task_info.task_name}]"))
            
            for frame in coro_info.call_stack:
                # Convert FrameInfo to tuple format (filename, lineno, funcname)
                # Add [async] prefix to function name for async frames
                current_stack.append((frame.filename, frame.lineno, f"[async] {frame.funcname}"))
            
            # If this task is awaited by others, append their stacks
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
                            # Combine current stack with each awaiter stack
                            for awaiter_stack in awaiter_stacks:
                                # Current stack is the leaf, awaiter stack is towards root
                                full_stack = current_stack + awaiter_stack
                                full_stacks.append(full_stack)
                        else:
                            # Awaiter has no further awaiters, use its coroutine stack
                            for awaiter_task_coro in awaiter_task.coroutine_stack:
                                awaiter_frames = []
                                # Add awaiter task name if meaningful
                                if awaiter_task.task_name and not awaiter_task.task_name.startswith('Task-'):
                                    awaiter_frames.append(("<task>", 0, f"[Task:{awaiter_task.task_name}]"))
                                for frame in awaiter_task_coro.call_stack:
                                    awaiter_frames.append((frame.filename, frame.lineno, f"[async] {frame.funcname}"))
                                full_stack = current_stack + awaiter_frames
                                full_stacks.append(full_stack)
                    else:
                        # Can't find awaiter task, just use the awaiter coroutine's stack
                        awaiter_frames = []
                        for frame in awaiter_coro.call_stack:
                            awaiter_frames.append((frame.filename, frame.lineno, f"[async] {frame.funcname}"))
                        full_stack = current_stack + awaiter_frames
                        full_stacks.append(full_stack)
            else:
                # No awaiters, this is a top-level task
                full_stacks.append(current_stack)
        
        visited.remove(task_info.task_id)
        return full_stacks
    
    def _update_stats_from_stack(self, stack):
        """Update stats from a reconstructed async stack."""
        if not stack:
            return
            
        # Process the stack similar to regular stack processing
        # The first frame (index 0) is the currently executing function (leaf)
        for i, frame in enumerate(stack):
            func_key = frame  # (filename, lineno, funcname)
            
            # Count direct calls (only for the first frame - the leaf)
            if i == 0:
                if func_key not in self.stats:
                    self.stats[func_key] = [0, 0, 0.0, 0.0, {}]
                self.stats[func_key][0] += 1  # direct calls
                self.stats[func_key][2] += self.sample_interval_sec  # total time
            
            # Count cumulative calls (for all frames in stack)
            if func_key not in self.stats:
                self.stats[func_key] = [0, 0, 0.0, 0.0, {}]
            self.stats[func_key][1] += 1  # cumulative calls
            self.stats[func_key][3] += self.sample_interval_sec  # cumulative time
            
            # Track caller relationships (frame i is called by frame i+1)
            if i < len(stack) - 1:
                caller = stack[i + 1]
                if caller not in self.stats[func_key][4]:
                    self.stats[func_key][4][caller] = [0, 0, 0.0, 0.0]
                self.stats[func_key][4][caller][0] += 1
                self.stats[func_key][4][caller][2] += self.sample_interval_sec