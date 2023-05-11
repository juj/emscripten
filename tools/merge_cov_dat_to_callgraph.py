import json, os, sys, subprocess

args = sys.argv.copy()

if len(args) <= 2:
  print('Usage: python ' + args[0] + ' --wasm filename.wasm --cov filename.cov --graph filename.callgraph.json -o filename.callgraph.json')
  sys.exit(0)

def extract_arg(optname):
  global args
  for i in range(len(args)):
    if args[i] == optname:
      output = args[i+1]
      args = args[:i] + args[i+2:]
      return output

def exit(reason):
  print('Usage: python ' + args[0] + ' --wasm filename.wasm --cov filename.cov --graph filename.callgraph.json -o filename.callgraph.json')
  print(reason)
  sys.exit(1)

wasm_filename = extract_arg('--wasm')
if not wasm_filename:
  exit('Specify input wasm file with --wasm filename.wasm')

cov_filename = extract_arg('--cov')
if not cov_filename:
  exit('Specify input coverage file with --cov filename.cov')

callgraph_filename = extract_arg('--graph')
if not callgraph_filename:
  exit('Specify input call graph file with --graph filename.callgraph.json')

output_filename = extract_arg('-o') or extract_arg('--o') or extract_arg('--output')
if not output_filename:
  exit('Specify output call graph file with --o filename.callgraph.json')

# Find all function ordinals in the Wasm file
cur_script_dir = os.path.dirname(os.path.realpath(__file__))
cmd = ['node', os.path.join(cur_script_dir, 'size_report', 'size_report.js'), '--json', wasm_filename]
size_report_json = subprocess.check_output(cmd).decode('utf-8')
#print(str(size_report_json))
size_report_json = json.loads(size_report_json)
print(str(size_report_json))

# Map function name -> wasm ordinal from size_report.js
ordinals = {}

for f in size_report_json:
  if 'ordinal' in f:
    ordinals[f['name']] = f['ordinal']
print(str(ordinals))

cg = json.load(open(callgraph_filename))
cov = open(cov_filename, 'rb').read()

for fn in cg['functions']:
  fname = cg['functionNames'][fn['n']]
  if fname in ordinals:
    ordinal = ordinals[fname]
    called = (cov[ordinal >> 3] & (1 << (ordinal & 7))) != 0
    call = ' CALLED' if called else ''
    print(str(fname) + ' -> ' + str(ordinals[fname]) + call)
    if called:
      fn['x'] = 1
    print('Function ' + fname + ' was called!')
  else:
    print('WARNING: Ordinal for function ' + fname + ' was not found!')

open(output_filename, 'w').write(json.dumps(cg))
print('Wrote output file ' + output_filename)
#idx = 0
#for b in cov:
#  for i in range(8):
#    if (b & (1 << i)):
 #     cg[]
#    pass