import json, os, sys, subprocess, shlex
from tools import config


def read_response_file(response_filename):
  """Reads a response file, and returns the list of cmdline params found in the
  file.

  The encoding that the response filename should be read with can be specified
  as a suffix to the file, e.g. "foo.rsp.utf-8" or "foo.rsp.cp1252". If not
  specified, first UTF-8 and then Python locale.getpreferredencoding() are
  attempted.

  The parameter response_filename may start with '@'."""
  if response_filename.startswith('@'):
    response_filename = response_filename[1:]

  if not os.path.exists(response_filename):
    raise IOError("response file not found: %s" % response_filename)

  # Guess encoding based on the file suffix
  components = os.path.basename(response_filename).split('.')
  encoding_suffix = components[-1].lower()
  if len(components) > 1 and (encoding_suffix.startswith('utf') or encoding_suffix.startswith('cp') or encoding_suffix.startswith('iso') or encoding_suffix in ['ascii', 'latin-1']):
    guessed_encoding = encoding_suffix
  else:
    # On windows, recent version of CMake emit rsp files containing
    # a BOM.  Using 'utf-8-sig' works on files both with and without
    # a BOM.
    guessed_encoding = 'utf-8-sig'

  try:
    # First try with the guessed encoding
    with open(response_filename, encoding=guessed_encoding) as f:
      args = f.read()
  except (ValueError, LookupError): # UnicodeDecodeError is a subclass of ValueError, and Python raises either a ValueError or a UnicodeDecodeError on decode errors. LookupError is raised if guessed encoding is not an encoding.
    # If that fails, try with the Python default locale.getpreferredencoding()
    with open(response_filename) as f:
      args = f.read()

  args = shlex.split(args)

  return args

def substitute_response_files(args):
  """Substitute any response files found in args with their contents."""
  new_args = []
  for arg in args:
    if arg.startswith('@'):
      new_args += read_response_file(arg)
    elif arg.startswith('-Wl,@'):
      for a in read_response_file(arg[5:]):
        if a.startswith('-'):
          a = '-Wl,' + a
        new_args.append(a)
    else:
      new_args.append(arg)
  return new_args

args = substitute_response_files(sys.argv[1:])

def extract_arg(optname):
  global args
  for i in range(len(args)):
    if args[i] == optname:
      output = args[i+1]
      args = args[:i] + args[i+2:]
      return output

out_json_filename = extract_arg('-o')
wasm_output_name = extract_arg('--wasm')

wasm_module_function_sizes = None

export_names = set()
import_names = set()

if wasm_output_name:
  wasm_module_function_sizes = {}
  wasm_opt = os.path.join(config.BINARYEN_ROOT, 'bin', 'wasm-opt')
  wasm_fnames = subprocess.check_output([wasm_opt, "--nm", wasm_output_name]).decode('utf-8')
  for line in wasm_fnames.split('\n'):
    if ':' in line:
      idx = line.rfind(':')
      fname = line[:idx].strip()
      fsize = int(line[idx+1:].strip())
      wasm_module_function_sizes[fname] = fsize


  # Find all imports and exports
  cur_script_dir = os.path.dirname(os.path.realpath(__file__))
  cmd = config.NODE_JS + [os.path.join(cur_script_dir, 'tools', 'size_report', 'size_report.js'), '--json', wasm_output_name]
  print(' '.join(cmd))
  size_report_json = subprocess.check_output(cmd).decode('utf-8')
  print(str(size_report_json))
  size_report_json = json.loads(size_report_json)

  for e in size_report_json:
    if e['type'] == 'import': import_names.add(e['name'])
    if e['type'] == 'export': export_names.add(e['name'])

  print('IMPORTS: ' + str(import_names))
  print('EXPORTS: ' + str(export_names))
  print('IMPLEMENTED FUNCTIONS: ' + str(wasm_module_function_sizes))

print('Merging ' + str(len(args)) + ' call graphs into one output: ' + out_json_filename)

graphs = []
for i in args:
  print('Loading input callgraph JSON ' + i)
  graphs += [json.load(open(i))]

filenames = {'': 0}

def record_filename(filename):
  assert filename != None
  if filename in filenames:
    return filenames[filename]
  id = len(filenames.keys())
  filenames[filename] = id
  return id

function_names = {'': 0}

def record_function_name(function_name):
  if function_name in function_names:
    return function_names[function_name]
  id = len(function_names.keys())
  function_names[function_name] = id
  return id

functions = []

# Merge all functions
for g in graphs:
  g_function_names = g['functionNames']
  g_filenames = g['filenames']
  for f in g['functions']:
#    print(str(f))
    name = g_function_names[f['n']]
    # Special name demangling that Binaryen pass does for 'main':
    if name == '__main_argc_argv':
      name = 'main'

    size = None
    if wasm_module_function_sizes is not None:
      if name not in wasm_module_function_sizes:
        continue
      size = wasm_module_function_sizes[name]

    name_number = record_function_name(name)
    filename = g_filenames[f['f']] if 'f' in f else None
    filename_number = record_filename(filename) if filename else 0
    line_number = f['l'] if 'l' in f else None

    callees = []
    callees_seen = set() # Deduplicate entries of a function calling another function several times
    if 'c' in f:
      for c in f['c']:
        if c['n'] in callees_seen:
          continue
        callees_seen.add(c['n'])
        callee_function_name = g_function_names[c['n']]
        callee_function_name_number = record_function_name(callee_function_name)
        call_line = c['l'] if 'l' in c else None
        call_column = c['c'] if 'c' in c else None
        callee = {
          'n': callee_function_name_number
        }
        if call_line: callee['l'] = call_line
        if call_column: callee['c'] = call_column
        callees += [callee]

    function = {
      'n': name_number
    }
    if name in import_names:
      print(name + ' IS AN IMPORT')
      function['import'] = 1

    if name in export_names:
      print(name + ' IS AN EXPORT')
      function['export'] = 1
    if size: function['s'] = size
    if filename_number: function['f'] = filename_number
    if line_number: function['l'] = line_number

    if len(callees) > 0:
      function['c'] = callees

    functions += [function]

def dict_to_linear_array(d):
  arr = ['']*len(d.keys())
  for key in d:
    arr[d[key]] = key
  return arr

function_names_array = dict_to_linear_array(function_names)
filenames_array = dict_to_linear_array(filenames)

# Find all unexpected roots (functions that are not called by any other function, these are likely referenced via a function pointer)
called_functions = set()
for f in functions:
  if 'c' in f:
    for c in f['c']:
      called_functions.add(c['n'])

for f in functions:
  if f['n'] not in called_functions:
    f['r'] = 1
    if 'export' not in f:
      print('Function "' + function_names_array[f['n']] + '" from file ' + (filenames_array[f['f']] if 'f' in f else 'UNKNOWN') + ' is an unexpected ROOT')

output_json = {
  'functionNames': function_names_array,
  'filenames': filenames_array,
  'functions': functions
}

def is_function_implemented(funcname):
  for f in output_json['functions']:
    if output_json['functionNames'][f['n']] == funcname:
      return True

# Do a double check that we got everything
if wasm_module_function_sizes is not None:
  for key in wasm_module_function_sizes:
    if not is_function_implemented(key):
      print('WARNING: Function ' + key + ' that is present in the .wasm file somehow did not make its way to the callgraph JSON!')

open(out_json_filename, 'w').write(json.dumps(output_json))
