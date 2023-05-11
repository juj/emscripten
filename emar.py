#!/usr/bin/env python3
# Copyright 2016 The Emscripten Authors.  All rights reserved.
# Emscripten is available under two separate licenses, the MIT license and the
# University of Illinois/NCSA Open Source License.  Both these licenses can be
# found in the LICENSE file.

"""Wrapper script around `llvm-ar`.
"""

import os, sys
from tools import shared, building
from tools.response_file import substitute_response_files

args = substitute_response_files(sys.argv)

# Merge call graph JSON files for the archive
input_files = []
output_file = None
for i in range(1, len(args)):
  if len(args[i]) >= 1 and len(args[i]) <= 3 and args[i].isalpha():
    input_files = args[i+2:]
    output_file = args[i+1]
    break

if output_file != None:
  json_files = []
  for i in input_files:
    f = i + '.callgraph.json'
    if os.path.isfile(f):
      json_files += [f]

  if len(json_files) > 0:
    print('emar.py: Merging ' + str(len(json_files)) + ' call graph JSON files to output file ' + output_file + '.callgraph.json')
    building.merge_call_graph_jsons(output_file + '.callgraph.json', json_files)
  else:
    print('emar.py: No input callgraph JSON files to generate ' + output_file + '.callgraph.json')
else:
  print('emar.py: Could not detect output file from ' + str(args))

cmd = [shared.LLVM_AR] + sys.argv[1:]
sys.exit(shared.run_process(cmd, stdin=sys.stdin, check=False).returncode)
