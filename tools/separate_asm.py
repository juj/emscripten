# Copyright 2015 The Emscripten Authors.  All rights reserved.
# Emscripten is available under two separate licenses, the MIT license and the
# University of Illinois/NCSA Open Source License.  Both these licenses can be
# found in the LICENSE file.

'''
Separates out the core asm module out of an emscripten output file.

This is useful because it lets you load the asm module first, then the main script, which on some browsers uses less memory
'''

import os, sys

sys.path.insert(1, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from tools import asm_module

infile = sys.argv[1]
asmfile = sys.argv[2]
otherfile = sys.argv[3]
asm_module_name = sys.argv[4]
if asm_module_name.startswith('var ') or asm_module_name.startswith('let '):
  asm_module_name_without_var = asm_module_name[4:]
else:
  asm_module_name_without_var = asm_module_name


everything = open(infile).read()
module = asm_module.AsmModule(infile).asm_js

module = module[module.find('=')+1:] # strip the initial "var asm =" bit, leave just the raw module as a function
if 'var Module' in everything:
  everything = everything.replace(module, asm_module_name_without_var)
else:
  # closure compiler removes |var Module|, we need to find the closured name
  # seek a pattern like (e.ENVIRONMENT), which is in the shell.js if-cascade for the ENVIRONMENT override
  import re
  m = re.search('\((\w+)\.ENVIRONMENT\)', everything)
  if not m:
    m = re.search('(\w+)\s*=\s*"__EMSCRIPTEN_PRIVATE_MODULE_EXPORT_NAME_SUBSTITUTION__"', everything)
  assert m, 'cannot figure out the closured name of Module statically'+ everything
  closured_name = m.group(1)
  everything = everything.replace(module, closured_name + '["asm"]')

o = open(asmfile, 'w')
o.write(asm_module_name + ' = ')
o.write(module)
o.write(';')
o.close()

o = open(otherfile, 'w')
o.write(everything)
o.close()

