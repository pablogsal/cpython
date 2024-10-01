#!/usr/bin/env python3
import os
import select
import sys

# Define FIFO paths (same as in the debug script)
INPUT_FIFO = "/tmp/pdb_input_fifo"
OUTPUT_FIFO = "/tmp/pdb_output_fifo"


def read_fifo(fifo, timeout=0.1):
    rlist, _, _ = select.select([fifo], [], [], timeout)
    if rlist:
        return fifo.readline().strip()
    return None


def main():
    if not os.path.exists(INPUT_FIFO) or not os.path.exists(OUTPUT_FIFO):
        print("Error: FIFOs not found. Make sure the debug script is running.")
        sys.exit(1)

    print("PDB FIFO Driver")
    print("Type 'quit' or 'exit' to end the session.")

    with open(INPUT_FIFO, "w") as input_fifo, open(OUTPUT_FIFO, "r") as output_fifo:
        while True:
            # Check for any pending output
            while True:
                output = read_fifo(output_fifo)
                if output is None:
                    break
                print(output)

            # Get user input
            try:
                command = input("(Pdb) ")
            except EOFError:
                break

            if command.lower() in ("quit", "exit"):
                break

            # Send command to PDB
            input_fifo.write(command + "\n")
            input_fifo.flush()

            # Wait for and print the response
            while True:
                output = read_fifo(output_fifo)
                if output is None:
                    break
                print(output)

    print("PDB session ended.")


if __name__ == "__main__":
    main()
