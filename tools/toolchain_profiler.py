import subprocess, os, time, sys, tempfile

EM_PROFILE_TOOLCHAIN = int(os.getenv('EM_PROFILE_TOOLCHAIN')) if os.getenv('EM_PROFILE_TOOLCHAIN') != None else 0

def remove_last_occurrence_if_exists(lst, item):
  for i in xrange(len(lst)):
    if lst[i] == item:
      lst.pop(i)
      return True
  return False

if EM_PROFILE_TOOLCHAIN:
  class ToolchainProfiler:
    # Provide a running counter towards negative numbers for PIDs for which we don't know what the actual process ID is
    imaginary_pid_ = 0

    @staticmethod
    def timestamp():
      return '{0:.3f}'.format(time.time())

    @staticmethod
    def record_process_start():
      ToolchainProfiler.mypid = str(os.getpid())
      profiler_logs_path = os.path.join(tempfile.gettempdir(), 'emscripten_toolchain_profiler_logs')
      try:
        os.makedirs(profiler_logs_path)
      except:
        pass
      ToolchainProfiler.logfile = open(os.path.join(profiler_logs_path, 'toolchain_profiler.pid_' + ToolchainProfiler.mypid + '.json'), 'a')
      ToolchainProfiler.logfile.write('[\n')
      ToolchainProfiler.logfile.write('{"pid":' + ToolchainProfiler.mypid + ',"op":"start","time":' + ToolchainProfiler.timestamp() + ',"cmdLine":["' + '","'.join(sys.argv).replace('\\', '\\\\') + '"]}')

    @staticmethod
    def record_process_exit(returncode):
      ToolchainProfiler.exit_all_blocks()
      ToolchainProfiler.logfile.write(',\n{"pid":' + ToolchainProfiler.mypid + ',"op":"exit","time":' + ToolchainProfiler.timestamp() + ',"returncode":' + str(returncode) + '}')
      ToolchainProfiler.logfile.write('\n]\n')
      ToolchainProfiler.logfile.close()
      ToolchainProfiler.logfile = None

    @staticmethod
    def record_subprocess_spawn(process_pid, process_cmdline):
      ToolchainProfiler.logfile.write(',\n{"pid":' + ToolchainProfiler.mypid + ',"op":"spawn","targetPid":' + str(process_pid) + ',"time":' + ToolchainProfiler.timestamp() + ',"cmdLine":["' + '","'.join(process_cmdline).replace('\\', '\\\\') + '"]}')

    @staticmethod
    def record_subprocess_wait(process_pid):
      ToolchainProfiler.logfile.write(',\n{"pid":' + ToolchainProfiler.mypid + ',"op":"wait","targetPid":' + str(process_pid) + ',"time":' + ToolchainProfiler.timestamp() + '}')

    @staticmethod
    def record_subprocess_finish(process_pid, returncode):
      ToolchainProfiler.logfile.write(',\n{"pid":' + ToolchainProfiler.mypid + ',"op":"finish","targetPid":' + str(process_pid) + ',"time":' + ToolchainProfiler.timestamp() + ',"returncode":' + str(returncode) + '}')

    block_stack = []

    @staticmethod
    def enter_block(block_name):
      ToolchainProfiler.logfile.write(',\n{"pid":' + ToolchainProfiler.mypid + ',"op":"enterBlock","name":"' + block_name + '","time":' + ToolchainProfiler.timestamp() + '}')
      ToolchainProfiler.block_stack.append(block_name)

    @staticmethod
    def exit_block(block_name):
      if remove_last_occurrence_if_exists(ToolchainProfiler.block_stack, block_name):
        ToolchainProfiler.logfile.write(',\n{"pid":' + ToolchainProfiler.mypid + ',"op":"exitBlock","name":"' + block_name + '","time":' + ToolchainProfiler.timestamp() + '}')

    @staticmethod
    def exit_all_blocks():
      for b in ToolchainProfiler.block_stack[::-1]:
        ToolchainProfiler.exit_block(b)

    class ProfileBlock:
      def __init__(self, block_name):
        self.block_name = block_name

      def __enter__(self):
        ToolchainProfiler.enter_block(self.block_name)

      def __exit__(self, type, value, traceback):
        ToolchainProfiler.exit_block(self.block_name)

    @staticmethod
    def profile_block(block_name):
      return ToolchainProfiler.ProfileBlock(block_name)

    @staticmethod
    def imaginary_pid():
      ToolchainProfiler.imaginary_pid_ -= 1
      return ToolchainProfiler.imaginary_pid_

else:
  class ToolchainProfiler:
    @staticmethod
    def record_process_start():
      pass

    @staticmethod
    def record_process_exit(returncode):
      pass

    @staticmethod
    def record_subprocess_spawn(process_pid, process_cmdline):
      pass

    @staticmethod
    def record_subprocess_wait(process_pid):
      pass

    @staticmethod
    def record_subprocess_finish(process_pid, returncode):
      pass

    @staticmethod
    def enter_block(block_name):
      pass

    @staticmethod
    def exit_block(block_name):
      pass

    class ProfileBlock:
      def __init__(self, block_name):
        pass
      def __enter__(self):
        pass
      def __exit__(self, type, value, traceback):
        pass

    @staticmethod
    def profile_block(block_name):
      return ToolchainProfiler.ProfileBlock(block_name)

class ProfiledPopen:
  def __init__(self, args, bufsize=0, executable=None, stdin=None, stdout=None, stderr=None, preexec_fn=None, close_fds=False,
               shell=False, cwd=None, env=None, universal_newlines=False, startupinfo=None, creationflags=0):
    self.process = subprocess.Popen(args, bufsize, executable, stdin, stdout, stderr, preexec_fn, close_fds, shell, cwd, env, universal_newlines, startupinfo, creationflags)
    self.pid = self.process.pid
    ToolchainProfiler.record_subprocess_spawn(self.pid, args)

  def communicate(self, input=None):
    ToolchainProfiler.record_subprocess_wait(self.pid)
    output = self.process.communicate(input)
    self.returncode = self.process.returncode
    ToolchainProfiler.record_subprocess_finish(self.pid, self.returncode)
    return output

  def poll(self):
    return self.process.poll()

  def kill(self):
    return self.process.kill()

def profiled_sys_exit(returncode):
  ToolchainProfiler.record_process_exit(returncode)
  sys.exit(returncode)

def profiled_check_call(cmd, *args, **kw):
  pid = ToolchainProfiler.imaginary_pid()
  ToolchainProfiler.record_subprocess_spawn(pid, cmd)
  try:
    ret = subprocess.check_call(cmd, *args, **kw)
  except Exception, e:
    ToolchainProfiler.record_subprocess_finish(pid, e.returncode)
    raise e
  ToolchainProfiler.record_subprocess_finish(pid, 0)
  return ret

def profiled_check_output(cmd, *args, **kw):
  pid = ToolchainProfiler.imaginary_pid()
  ToolchainProfiler.record_subprocess_spawn(pid, cmd)
  try:
    ret = subprocess.check_output(cmd, *args, **kw)
  except Exception, e:
    ToolchainProfiler.record_subprocess_finish(pid, e.returncode)
    raise e
  ToolchainProfiler.record_subprocess_finish(pid, 0)
  return ret
