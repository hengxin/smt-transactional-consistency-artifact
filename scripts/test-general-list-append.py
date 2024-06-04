import os
import subprocess
import inspect, re
import time

def var_name(p):
  for line in inspect.getframeinfo(inspect.currentframe().f_back)[3]:
    m = re.search(r'\bvar_name\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)', line)
    if m:
      return m.group(1)


history_type = 'elle-list-append'
root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
# history_path = os.path.join(root_path, 'history', 'ser', 'general-list-append', 'general')
# history_path = os.path.join(root_path, 'history', 'ser', 'general-list-append', 'single-write-uv')
# history_path = os.path.join(root_path, 'history', 'ser', 'general-list-append', 'single-write-uv2')
history_path = os.path.join(root_path, 'history', 'ser', 'general-list-append', 'same-listappend-rw-big')
transform_script_path = os.path.join(root_path, 'scripts', 'edn2txt', 'edn2txt.py')
checker = 'nuser'
print(f'checker = {checker}')
if checker == 'elle':
  checker_path = '/home/rikka/elle-cli/target/elle-cli-0.1.7-standalone.jar' # this is the absolute path of the built PolySI
else:
  checker_path = os.path.join(root_path, 'builddir-release', 'checker')
  solver = 'acyclic-minisat'
  print('use [{}] as backend solver'.format(solver))

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
# same listappend and rw(big)
params = {
  'txn': ['20_250_10_8000_0.5_r_0.5_100', 
          '20_500_10_8000_0.5_r_0.5_100',
          '20_750_10_8000_0.5_r_0.5_100',
          '20_800_10_8000_0.5_r_0.5_100', 
          '20_900_10_8000_0.5_r_0.5_100', 
          '20_1000_10_8000_0.5_r_0.5_100'],
}

def run_single(history_dir, bincode):
  print('--- checking {}/{} ---'.format(history_dir, bincode))
  bincode_path = os.path.join(history_path, history_dir, bincode)
  runtime = 0
  if checker == 'elle':
    start_time = time.perf_counter()
    logs = subprocess.run(['java', '-jar', checker_path, '--model', 'list-append', '-f', 'edn', bincode_path, '-c', 'serializable'], capture_output=True, text=True).stdout.split(os.linesep)
    end_time = time.perf_counter()
    for log in logs:
      if log == '':
        continue
      # print(log)
      assert log.split(' ')[-1] == 'true'
    runtime = (end_time - start_time) * 1000
  else:
    output_tmp_file_name = 'hist.txt'
    output_tmp_file_path = os.path.join(history_path, history_dir, output_tmp_file_name)
    with open(output_tmp_file_path, 'w+') as hist_file:
      subprocess.run(['python3', transform_script_path, bincode_path], stdout=hist_file)
    start_time = time.perf_counter()
    logs = subprocess.run([checker_path, output_tmp_file_path, '--solver', solver, '--history-type', history_type, '--pruning', 'fast'], capture_output=True, text=True).stdout.split(os.linesep)
    end_time = time.perf_counter()
    for log in logs:
      if log == '':
        continue
      if log[0] == '[':
        if log.find(':') == -1:
          continue
        if log.strip().endswith('ms'):
          runtime += int(log.split(':')[-1].strip()[:-2]) # xxx'ms'
      elif log[0] == 'a': # accept
        if log.split(':')[-1].strip() != 'true':
          print(f'checking result of {history_dir}/{bincode} is false')
        # assert log.split(':')[-1].strip() == 'true' # must satisfy si
      # print(log)
    # print(runtime)
    # print((end_time - start_time) * 1000)
    runtime = (end_time - start_time) * 1000
    os.remove(output_tmp_file_path)
  return runtime


def run(history_dir):
  statistics = [ run_single(history_dir, bincode) for bincode in os.listdir(os.path.join(history_path, history_dir))]
  return sum(statistics) / len(statistics)

all_statistics = {}
for fig_id in params:
  print('name: {} '.format(fig_id))
  statistics = [run(h) for h in params[fig_id]]
  all_statistics['{}'.format(fig_id)] = statistics
print(all_statistics)