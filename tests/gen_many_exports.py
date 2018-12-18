#!/usr/bin/env python

# Copyright 2018 The Emscripten Authors.  All rights reserved.
# Emscripten is available under two separate licenses, the MIT license and the
# University of Illinois/NCSA Open Source License.  Both these licenses can be
# found in the LICENSE file.

import sys

NUM_FUNCS_TO_GENERATE = 1

def func_name(i):
  return 'thisIsAFunctionExportedFromAsmJsOrWasmWithVeryLongFunctionNameThatWouldBeGreatToOnlyHaveThisLongNamePresentAtMostOnceWhenExportedFromAsmJsOrWasmOtherwiseCodeSizesWillBeLargeAndNetworkTransfersBecomeVerySlowThatUsersWillGoAwayAndVisitSomeOtherSiteInsteadAndThenWebAssemblyDeveloperIsSadOrEvenWorseNobodyNoticesButInternetPipesWillGetMoreCongestedWhichContributesToGlobalWarmingAndThenEveryoneElseWillBeSadAsWellEspeciallyThePolarBearsAndPenguinsJustThinkAboutThePenguins' + str(i+1)

def generate_c_program_with_lots_of_exports(out_file):
  with open(out_file, 'w') as f:
    f.write('#include <stdio.h>\n\n')
    f.write('#include <emscripten.h>\n\n')

    for i in range(NUM_FUNCS_TO_GENERATE):
      f.write('int EMSCRIPTEN_KEEPALIVE ' + func_name(i) + '(void) { return ' + str(i+1) + '; }\n')

    f.write('\nint main() {\n')
    f.write('  int sum = 0;\n')

    for i in range(NUM_FUNCS_TO_GENERATE):
      f.write('  sum += ' + func_name(i) + '();\n')

    f.write('\n  printf("Sum of numbers from 1 to ' + str(NUM_FUNCS_TO_GENERATE) + ': %d (expected ' + str(int((NUM_FUNCS_TO_GENERATE * (NUM_FUNCS_TO_GENERATE+1))/2)) + ')\\n", sum);\n');
    f.write('}\n');

if __name__ == '__main__':
  generate_c_program_with_lots_of_exports(sys.argv[1])
