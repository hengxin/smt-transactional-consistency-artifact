import os
import subprocess
import inspect, re

def var_name(p):
  for line in inspect.getframeinfo(inspect.currentframe().f_back)[3]:
    m = re.search(r'\bvar_name\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)', line)
    if m:
      return m.group(1)


history_type = 'dbcop'
root_path = root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
history_path = os.path.join(root_path, 'history', 'si', 'polysi', 'fig7')
checker = 'nusi'
if checker == 'polysi':
  checker_path = '/home/rikka/PolySI/build/libs/PolySI-1.0.0-SNAPSHOT.jar' # this is the absolute path of the built PolySI
else:
  checker_path = os.path.join(root_path, 'builddir-release', 'checker')
  solver = 'acyclic-minisat'
  print('use [{}] as backend solver'.format(solver))

# params
params = {
  'a': ['5_100_15_10000_0.5_uniform', '10_100_15_10000_0.5_uniform', '15_100_15_10000_0.5_uniform', '20_100_15_10000_0.5_uniform', '25_100_15_10000_0.5_uniform', '30_100_15_10000_0.5_uniform'],
  'b': ['20_10_15_10000_0.5_uniform', '20_20_15_10000_0.5_uniform', '20_30_15_10000_0.5_uniform', '20_40_15_10000_0.5_uniform', '20_50_15_10000_0.5_uniform', '20_100_15_10000_0.5_uniform', '20_150_15_10000_0.5_uniform', '20_200_15_10000_0.5_uniform', '20_250_15_10000_0.5_uniform'],
  'c': ['20_100_5_10000_0.5_uniform', '20_100_10_10000_0.5_uniform', '20_100_15_10000_0.5_uniform', '20_100_20_10000_0.5_uniform', '20_100_25_10000_0.5_uniform', '20_100_30_10000_0.5_uniform'],
  'd': ['20_100_15_10000_0_uniform', '20_100_15_10000_0.25_uniform', '20_100_15_10000_0.5_uniform', '20_100_15_10000_0.75_uniform', '20_100_15_10000_1_uniform'],
  'e': ['20_100_15_1000_0.5_uniform', '20_100_15_2000_0.5_uniform', '20_100_15_3000_0.5_uniform', '20_100_15_4000_0.5_uniform', '20_100_15_6000_0.5_uniform', '20_100_15_8000_0.5_uniform', '20_100_15_10000_0.5_uniform'],
  'f': ['20_100_15_10000_0.5_uniform', '20_100_15_10000_0.5_zipf', '20_100_15_10000_0.5_hotspot']
}

def run_single(history_dir, bincode):
  print('--- checking {}/{} ---'.format(history_dir, bincode))
  bincode_path = os.path.join(history_path, history_dir, bincode, 'history.bincode')
  time = 0
  if checker == 'polysi':
    logs = subprocess.run(['java', '-jar', checker_path, 'audit', '--type={}'.format(history_type), bincode_path], capture_output=True, text=True).stderr.split(os.linesep)
    for log in logs:
      if log == '':
        continue
      if log.find(':') == -1:
        if log[0] == '[':
          assert log == '[[[[ ACCEPT ]]]]' # must satisfy si
        continue
      arg_name, arg = log.split(':')[0].strip(), log.split(':')[-1].strip()
      if arg_name == 'ENTIRE_EXPERIMENT':
        time += int(arg[:-2]) # xxx'ms'
  else:
    logs = subprocess.run([checker_path, bincode_path, '--solver', solver, '--history-type', history_type, '--pruning', 'fast'], capture_output=True, text=True).stdout.split(os.linesep)
    for log in logs:
      if log == '':
        continue
      if log[0] == '[':
        if log.find(':') == -1:
          continue
        if log.strip().endswith('ms'):
          time += int(log.split(':')[-1].strip()[:-2]) # xxx'ms'
      elif log[0] == 'a': # accept
        if log.split(':')[-1].strip() != 'true':
          print(f'checking result of {history_dir}/{bincode} is false')
        # assert log.split(':')[-1].strip() == 'true' # must satisfy si
  return time


def run(history_dir):
  statistics = [ run_single(history_dir, bincode) for bincode in os.listdir(os.path.join(history_path, history_dir))]
  return sum(statistics) / len(statistics)

all_statistics = {}
for fig_id in params:
  print('fig7{}: '.format(fig_id))
  statistics = [run(h) for h in params[fig_id]]
  all_statistics['fig7{}'.format(fig_id)] = statistics
print(all_statistics)
