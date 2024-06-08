#!/usr/bin/env python3

import argparse
import os
import signal
import subprocess
import sys

# Define command-line arguments
parser = argparse.ArgumentParser(description='Launch an external application multiple times and check its return value.')
parser.add_argument('-f', '--file', type=str, required=True, help='Path to the external application to launch.')
parser.add_argument('-n', '--num', type=int, default=7, help='Number of times to launch the application.')
parser.add_argument('-e', '--error', type=int, default=0, help='Expected return value for the subprocess.')

args = parser.parse_args()

# Check if the file exists and is executable
if not os.path.isfile(args.file) or not os.access(args.file, os.X_OK):
    print(f"Error: {args.file} is not a valid executable file.")
    exit(1)

# Define signal handler for SIGINT (Ctrl+C)
def signal_handler(signal, frame):
    print("\nReceived SIGINT, exiting...")
    # Propagate the signal to the subprocess
    os.killpg(os.getpgid(subprocess_obj.pid), signal.SIGINT)
    # Wait for the subprocess to finish
    subprocess_obj.wait()
    print(f" => subprocess exited with value {subprocess_obj.returncode}")
    exit(subprocess_obj.returncode)

# Register signal handler for SIGINT
signal.signal(signal.SIGINT, signal_handler)

# Number of times the application has crashed
num_crashes = 0
# Number of times the application has returned an unexpected value
num_failures = 0

# Launch the application num_launches times and check its return value
for i in range(1, args.num + 1):
    sys.stdout.write(f"Launching Iteration {i} / {args.num}... ")
    sys.stdout.flush()
    try:
        # Redirect standard output to /dev/null to avoid capturing it
        with open('/dev/null', 'w') as fnull:
            # Create a new process group for the subprocess
            subprocess_obj = subprocess.Popen([args.file], stdout=fnull, stderr=subprocess.STDOUT, preexec_fn=os.setpgrp)
            # Wait for the subprocess to finish
            subprocess_obj.wait()

            # Check if the application crashed or returned an unexpected value
            if subprocess_obj.returncode < 0:
                num_crashes += 1
                print(f"CRASH {subprocess_obj.returncode}")
            elif subprocess_obj.returncode != args.error:
                num_failures += 1
                print(f"FAILED {subprocess_obj.returncode}")
            else:
                print(f"OK {subprocess_obj.returncode}")
    except subprocess.CalledProcessError as e:
        # Catch CalledProcessError exceptions and print the error message
        num_failures += 1
        print(f"Iteration {i} failed with error: {str(e)}")
    except Exception as e:
        # Catch any other exceptions and print the error message
        num_crashes += 1
        print(f"Iteration {i} failed with error: {str(e)}")

# Print the total number of crashes and failures
print(f"number of crashes: {num_crashes}")
print(f"number of failures: {num_failures}")
print(f"Total number of issues (crash or failure) : {num_crashes + num_failures} / {args.num}")

