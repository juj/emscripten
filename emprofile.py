import sys, shutil, os, json, tempfile, time

profiler_logs_path = os.path.join(tempfile.gettempdir(), 'emscripten_toolchain_profiler_logs')

# Deletes all previously captured log files to make room for a new clean run.
def delete_profiler_logs():
  try:
    shutil.rmtree(profiler_logs_path)
  except:
    pass

def list_files_in_directory(d):
  files = []
  try:
    items = os.listdir(d)
    for i in items:
      f = os.path.join(d, i)
      if os.path.isfile(f):
        files += [f]
    return files
  except:
    return []

def create_profiling_graph():
  log_files = [f for f in list_files_in_directory(profiler_logs_path) if 'toolchain_profiler.pid_' in f]

  all_results = []
  if len(log_files) > 0:
    print 'Processing ' + str(len(log_files)) + ' profile log files in "' + profiler_logs_path + '"...'
  for f in log_files:
    all_results += json.loads(open(f, 'r').read())
  if len(all_results) == 0:
    print 'No profiler logs were found in path "' + profiler_logs_path + '". Try setting the environment variable EM_PROFILE_TOOLCHAIN=1 and run some emcc commands, and then rerun "python emprofile.py --graph" again.'
    return

  json_file = 'toolchain_profiler.results_' + time.strftime('%Y%m%d') + '.json'
  open(json_file, 'w').write(json.dumps(all_results))
  print 'Wrote "' + json_file + '"'

  html_file = json_file.replace('.json', '.html')
  html_contents = open(os.path.join(os.path.dirname(os.path.realpath(__file__)), 'tools', 'toolchain_profiler.results_template.html'), 'r').read().replace('{{{results_log_file}}}', '"' + json_file + '"')
  open(html_file, 'w').write(html_contents)
  print 'Wrote "' + html_file + '"'

  delete_profiler_logs()

if sys.argv[1] == '--reset':
  delete_profiler_logs()
elif sys.argv[1] == '--graph':
  create_profiling_graph()
