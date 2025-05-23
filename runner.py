#!/usr/bin/env python3
"""
Test target script - a CPU intensive Python program to benchmark.
Run this script and then use the benchmark profiler on its PID.

Usage:
1. Run this script: python test_target.py
2. Get its PID from the output
3. Run the benchmark: python benchmark.py --benchmark <PID>
"""

import time
import os
import sys
import math

def slow_fibonacci(n):
    """Intentionally slow recursive fibonacci - should show up prominently in profiler"""
    if n <= 1:
        return n
    return slow_fibonacci(n-1) + slow_fibonacci(n-2)

def medium_computation():
    """Medium complexity function"""
    result = 0
    for i in range(1000):
        result += math.sqrt(i) * math.sin(i)
    return result

def fast_loop():
    """Fast simple loop"""
    total = 0
    for i in range(100):
        total += i
    return total

def string_operations():
    """String manipulation that should be visible in profiler"""
    text = "hello world " * 100
    words = text.split()
    return " ".join(reversed(words))

def nested_calls():
    """Nested function calls to test call stack depth"""
    def level1():
        def level2():
            def level3():
                return medium_computation()
            return level3()
        return level2()
    return level1()

def main_loop():
    """Main computation loop with different execution paths"""
    iteration = 0

    while True:
        iteration += 1

        # Different execution paths with different frequencies
        if iteration % 50 == 0:
            # Expensive operation - should show high per-call time
            result = slow_fibonacci(20)

        elif iteration % 10 == 0:
            # Medium operation
            result = nested_calls()

        elif iteration % 5 == 0:
            # String operations
            result = string_operations()

        else:
            # Fast operation - most common
            result = fast_loop()

        # Small delay to make sampling more interesting
        time.sleep(0.001)


def main():
    print(f"Test target script starting...")
    print(f"PID: {os.getpid()}")
    print(f"Use this command to benchmark:")
    print(f"python benchmark.py --benchmark {os.getpid()}")
    print()
    print("Starting main loop... Press Ctrl+C to stop")

    try:
        main_loop()
    except KeyboardInterrupt:
        print("\nStopping...")

if __name__ == "__main__":
    main()
