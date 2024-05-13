import re
import sys
import matplotlib.pyplot as plt


def count_function_sizes(input_text):
    # Regular expression pattern to match function names
    pattern = re.compile(r'py::(.*?):')

    # Dictionary to store function names and their counts
    function_counts = {}

    for line in input_text.split('\n'):
        match = pattern.search(line)
        if match:
            function_name = match.group(1)
            function_counts[function_name] = function_counts.get(
                function_name, 0) + 1

    # Sort the function counts by count in descending order, and then by function name
    sorted_function_counts = sorted(
        function_counts.items(), key=lambda x: (-x[1], x[0]))

    # Select only the top 10 functions
    return dict(sorted_function_counts[:10])


def plot_function_counts(function_counts):
    function_names = list(function_counts.keys())
    counts = list(function_counts.values())

    plt.figure(figsize=(10, 6))
    plt.barh(function_names, counts, color='skyblue')
    plt.xlabel('Count')
    plt.ylabel('Function Name')
    plt.title('Top 10 Function Counts')
    # Invert y-axis to display the function with the highest count on top
    plt.gca().invert_yaxis()
    plt.tight_layout()
    plt.show()


def main():
    input_text = sys.stdin.read()
    function_counts = count_function_sizes(input_text)

    plot_function_counts(function_counts)


if __name__ == "__main__":
    main()

