import os
import subprocess
import inspect, re
import time
import signal
import tempfile

def var_name(p):
  for line in inspect.getframeinfo(inspect.currentframe().f_back)[3]:
    m = re.search(r'\bvar_name\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)', line)
    if m:
      return m.group(1)

TO = 10 * 60 # 600s
TIME_BIN = '/usr/bin/time'
history_type = 'elle-list-append'
root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
# history_path = os.path.join(root_path, 'history', 'ser', 'general-list-append', 'general')
# history_path = os.path.join(root_path, 'history', 'ser', 'general-list-append', 'single-write-uv')
# history_path = os.path.join(root_path, 'history', 'ser', 'general-list-append', 'single-write-uv2')
# history_path = os.path.join(root_path, 'history', 'ser', 'general-list-append', 'list-rw-no-single-write')
history_path = os.path.join(root_path, 'history', 'ser', 'general-list-append', 'list-rw-various')
transform_script_path = os.path.join(root_path, 'scripts', 'edn2txt', 'edn2txt.py')
# checker = 'elle'
checker = 'nuser'
# mode = 'rw' 
mode = 'list'
print(f'checker = {checker}')
if checker == 'elle':
  checker_path = '/home/rikka/elle-cli/target/elle-cli-0.1.7-standalone.jar' # this is the absolute path of the built PolySI
else:
  checker_path = os.path.join(root_path, 'builddir-release', 'checker')
  if mode == 'rw':
    checker_path = "/home/rikka/smt-transactional-consistency-artifact/builddir-release/checker"
  solver = 'acyclic-minisat'
  print('use [{}] as backend solver'.format(solver))
  print(f'mode = {mode}')

# params
# general and single-write-uv
# params = {
#   'op': ['op2', 'op5', 'op10'],
#   'session': ['sess2', 'sess5', 'sess10', 'sess15', 'sess20'],
#   'txn': ['txns-per-session50', 'txns-per-session100', 'txns-per-session150', 'txns-per-session200', 'txns-per-session250'],
# }
# single-write-uv2
# params = {
#   'op': ['op2', 'op5', 'op10', 'op15', 'op20'],
#   'session': ['sess5', 'sess10', 'sess15', 'sess20', 'sess25'],
#   'txn': ['txns-per-session50', 'txns-per-session100', 'txns-per-session200', 'txns-per-session300', 'txns-per-session400', 'txns-per-session500'],
# }

# same listappend and rw
# params = {
#   'txn': ['20_100_10_2000_0.5_r_0.5_100', 
#           '20_200_10_2000_0.5_r_0.5_100',
#           '20_300_10_2000_0.5_r_0.5_100',
#           '20_400_10_2000_0.5_r_0.5_100', 
#           '20_500_10_2000_0.5_r_0.5_100'] 
# }
# same listappend and rw(big)
# params = {
#   'txn': ['20_250_10_8000_0.5_r_0.5_100', 
#           '20_500_10_8000_0.5_r_0.5_100',
#           '20_750_10_8000_0.5_r_0.5_100',
#           '20_800_10_8000_0.5_r_0.5_100', 
#           '20_900_10_8000_0.5_r_0.5_100', 
#           '20_1000_10_8000_0.5_r_0.5_100'],
# }
# same listappend and rw2
# params = {
#   'txn': ['20_500_10_2000_0.5_r_0.5_100',
#           '20_750_10_2000_0.5_r_0.5_100',
#           '20_800_10_2000_0.5_r_0.5_100', 
#           '20_900_10_2000_0.5_r_0.5_100', 
#           '20_1000_10_2000_0.5_r_0.5_100'],
# }

# list-rw
params = {
  # 'dup-r': ['100_100_8_5000_0.5_r_0_1.5_100',
  #           '100_100_8_5000_0.5_r_0.25_1.5_100',
  #           '100_100_8_5000_0.5_r_0.5_1.5_100', 
  #           '100_100_8_5000_0.5_r_0.75_1.5_100', 
  #           '100_100_8_5000_0.5_r_1_1.5_100'],
  
  "sess" : ["5_100_20_5000_0.5_r_0.5_0.5_100",
            "10_100_20_5000_0.5_r_0.5_0.5_100",
            "15_100_20_5000_0.5_r_0.5_0.5_100",
            "20_100_20_5000_0.5_r_0.5_0.5_100",
            "25_100_20_5000_0.5_r_0.5_0.5_100",
            "30_100_20_5000_0.5_r_0.5_0.5_100", ],
  
  "txn" : ["20_10_20_5000_0.5_r_0.5_0.5_100",
           "20_20_20_5000_0.5_r_0.5_0.5_100",
           "20_30_20_5000_0.5_r_0.5_0.5_100",
           "20_40_20_5000_0.5_r_0.5_0.5_100",
           "20_50_20_5000_0.5_r_0.5_0.5_100",
           "20_100_20_5000_0.5_r_0.5_0.5_100",
           "20_150_20_5000_0.5_r_0.5_0.5_100",
           "20_200_20_5000_0.5_r_0.5_0.5_100",
           "20_250_20_5000_0.5_r_0.5_0.5_100",],
  
  "ops" : ["20_100_5_5000_0.5_r_0.5_0.5_100",
           "20_100_10_5000_0.5_r_0.5_0.5_100",
           "20_100_15_5000_0.5_r_0.5_0.5_100",
           "20_100_20_5000_0.5_r_0.5_0.5_100",
           "20_100_25_5000_0.5_r_0.5_0.5_100",
           "20_100_30_5000_0.5_r_0.5_0.5_100",],
  
  
  "keys" : ["20_100_20_2000_0.5_r_0.5_0.5_100",
            "20_100_20_4000_0.5_r_0.5_0.5_100",
            "20_100_20_6000_0.5_r_0.5_0.5_100",
            "20_100_20_8000_0.5_r_0.5_0.5_100",
            "20_100_20_10000_0.5_r_0.5_0.5_100",],
  
  "read-r" : ["20_100_20_5000_0.05_r_0.5_0.5_100",
              "20_100_20_5000_0.25_r_0.5_0.5_100",
              "20_100_20_5000_0.5_r_0.5_0.5_100",
              "20_100_20_5000_0.75_r_0.5_0.5_100",
              "20_100_20_5000_0.95_r_0.5_0.5_100",],
  
  "dup-r" : ["20_100_20_5000_0.5_r_0_0.5_100",
             "20_100_20_5000_0.5_r_0.2_0.5_100",
             "20_100_20_5000_0.5_r_0.4_0.5_100",
             "20_100_20_5000_0.5_r_0.6_0.5_100",
             "20_100_20_5000_0.5_r_0.8_0.5_100",
             "20_100_20_5000_0.5_r_1.0_0.5_100",],
}

def read_max_rss_kb(time_output_path):
  with open(time_output_path) as time_output:
    for line in time_output:
      if line.startswith('max_rss_kb='):
        value = line.split('=', 1)[1].strip()
        if value:
          return int(value)
  return None

def run_command_with_memory(cmd, timeout=None):
  with tempfile.NamedTemporaryFile(delete=False) as time_output:
    time_output_path = time_output.name
  try:
    timed_cmd = [TIME_BIN, '-f', 'max_rss_kb=%M', '-o', time_output_path] + cmd
    start_time = time.perf_counter()
    process = subprocess.Popen(timed_cmd,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE,
                               text=True,
                               start_new_session=True)
    try:
      stdout, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
      try:
        os.killpg(process.pid, signal.SIGKILL)
      except ProcessLookupError:
        pass
      process.communicate()
      raise
    end_time = time.perf_counter()
    return stdout.split(os.linesep), (end_time - start_time) * 1000, read_max_rss_kb(time_output_path)
  finally:
    os.remove(time_output_path)

def run_single(history_dir, bincode):
  print('--- checking {}/{} ---'.format(history_dir, bincode))
  bincode_path = os.path.join(history_path, history_dir, bincode)
  runtime = 0
  max_rss_kb = None
  if checker == 'elle':
    cmd = ['java', '-jar', checker_path, '--model', 'list-append', '-f', 'edn', bincode_path, '-c', 'serializable']
    logs, runtime, max_rss_kb = run_command_with_memory(cmd)
    for log in logs:
      if log == '':
        continue
      # print(log)
      assert log.split(' ')[-1] == 'true'
  else:
    if mode == 'list':
      output_tmp_file_name = 'hist.txt'
      output_tmp_file_path = os.path.join(history_path, history_dir, output_tmp_file_name)
      with open(output_tmp_file_path, 'w+') as hist_file:
        subprocess.run(['python3', transform_script_path, bincode_path], stdout=hist_file)
      cmd = [checker_path, output_tmp_file_path, '--solver', solver, '--history-type', history_type, '--pruning', 'fast']
      logs, runtime, max_rss_kb = run_command_with_memory(cmd, timeout=TO)
      for log in logs:
        if log == '':
          continue
        if log[0] == '[':
          if log.find(':') == -1:
            continue
          if log.strip().endswith('ms'):
            continue
        elif log[0] == 'a': # accept
          if log.split(':')[-1].strip() != 'true':
            print(f'checking result of {history_dir}/{bincode} is false')
          # assert log.split(':')[-1].strip() == 'true' # must satisfy si
        # print(log)
      # print(runtime)
      # print((end_time - start_time) * 1000)
      os.remove(output_tmp_file_path)
    elif mode == "rw":
      bincode_path = os.path.join(bincode_path, 'history.bincode')
      cmd = [checker_path, bincode_path, '--solver', solver, '--pruning', 'fast']
      logs, runtime, max_rss_kb = run_command_with_memory(cmd, timeout=TO)
      for log in logs:
        if log == '':
          continue
        if log[0] == '[':
          if log.find(':') == -1:
            continue
          if log.strip().endswith('ms'):
            continue
        elif log[0] == 'a': # accept
          if log.split(':')[-1].strip() != 'true':
            print(f'checking result of {history_dir}/{bincode} is false')
          # assert log.split(':')[-1].strip() == 'true' # must satisfy si
        # print(log)
      # print(runtime)
      # print((end_time - start_time) * 1000)
  # print('max_rss_kb = {}'.format(max_rss_kb))
  return runtime, max_rss_kb

def average(values):
  values = [value for value in values if value is not None]
  if len(values) == 0:
    return None
  return sum(values) / len(values)


def run(history_dir):
  if checker == 'elle' or (checker == 'nuser' and mode == 'list'):
    statistics = [run_single(history_dir, bincode)
                for bincode in os.listdir(os.path.join(history_path, history_dir)) 
                if os.path.isfile(os.path.join(history_path, history_dir, bincode))]
  else: # checker == 'nuser' and mode == 'rw'
    statistics = [run_single(history_dir, bincode)
                for bincode in os.listdir(os.path.join(history_path, history_dir)) 
                if os.path.isdir(os.path.join(history_path, history_dir, bincode))]
  runtimes = [runtime for runtime, _ in statistics]
  max_rss_values = [max_rss_kb for _, max_rss_kb in statistics]
  return average(runtimes), average(max_rss_values)

all_statistics = {}
all_memory_statistics = {}
for fig_id in params:
  print('name: {} '.format(fig_id))
  statistics = [run(h) for h in params[fig_id]]
  all_statistics['{}'.format(fig_id)] = [runtime for runtime, _ in statistics]
  all_memory_statistics['{}'.format(fig_id)] = [max_rss_kb for _, max_rss_kb in statistics]
print('runtime_ms_statistics:')
print(all_statistics)
print('max_rss_kb_statistics:')
print(all_memory_statistics)
