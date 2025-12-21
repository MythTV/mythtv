#!/usr/bin/python3

import sys
import json
from pathlib import Path

#
# Filter out entries starting whose "output" line contains any of the
# listed strings.
#
filters = [
    'external/libmythdvdnav',
    '/mocs_',
    ]



def read_cc_file(directory):
    """ Open and read a compile_commands.json file"""
    directory_path = Path(directory)
    filename = directory_path / 'compile_commands.json'
    if verbose:
        print(f"Reading: {filename}")
    try:
        with open(filename, 'r') as file:
            data = json.load(file)
    except FileNotFoundError:
        print("Error: Not found: ${filename}")
    except json.JSONDecodeError:
        print("Error: Not JSON: ${filename}")
    return data

def write_cc_file(*, directory, data):
    """ Open and write a compile_commands.json file"""
    directory_path = Path(directory)
    filename = directory_path / 'compile_commands.json'
    if verbose:
        print(f"Writitng: {filename}")
    try:
        with open(filename, 'w') as file:
            data = json.dump(data, file, indent=2)
    except PermissionError:
        print("Error: Permission denied: ${filename}")
    except OSError as e:
        print("Error: I/O error {e}: ${filename}")



#
# Process command line arguments
#
verbose = (sys.argv[1] == '-v')
if verbose:
    del sys.argv[1]
    print(f"Command: {sys.argv[0]}")
    print(f"Output dir: {sys.argv[1]}")
    print(f"MythTV dir: {sys.argv[2]}")
    print(f"Plugin dir: {sys.argv[3]}")

#
# Read and filter MythTV compile_commands.json
#
tv_cc_data = read_cc_file(sys.argv[2])
if verbose:
    print(f"Length of MythTV is: {len(tv_cc_data)}")
for filter in filters:
    tv_cc_data = [ x for x in tv_cc_data if filter not in x['output'] ]
    if verbose:
        print(f"Filtered MythTV is:  {len(tv_cc_data)} after '{filter}'")

#
# Read MythPlugins compile_commands.json
#
pl_cc_data = read_cc_file(sys.argv[3])
if verbose:
    print(f"Length of Plugins is: {len(pl_cc_data)}")
for filter in filters:
    pl_cc_data = [ x for x in pl_cc_data if filter not in x['output'] ]
    if verbose:
        print(f"Filtered Plugins is:  {len(pl_cc_data)} after '{filter}'")

#
# Write the combined list.
#
all_cc_data = tv_cc_data + pl_cc_data
write_cc_file(directory=sys.argv[1], data=all_cc_data)
if verbose:
    print(f"Length of Output is: {len(all_cc_data)}")
