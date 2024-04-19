import os
import subprocess
import inspect, re

def var_name(p):
  for line in inspect.getframeinfo(inspect.currentframe().f_back)[3]:
    m = re.search(r'\bvar_name\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)', line)
    if m:
      return m.group(1)


history_type = 'cobra'
root_path = root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
history_path = '/home/rikka/viper/Viper/history_data2/logs/fig8'
checker = 'nusi'
if checker == 'polysi':
  checker_path = '/home/rikka/PolySI/build/libs/PolySI-1.0.0-SNAPSHOT.jar' # this is the absolute path of the built PolySI
elif checker == 'nusi':
  checker_path = os.path.join(root_path, 'builddir-release', 'checker')
  solver = 'acyclic-minisat'
elif checker == 'viper':
  # checker_path = 
  print('use [{}] as backend solver'.format(solver))

# params
history_names = {
  1000 : 'cheng_normal-Cheng-1000txns-8oppertxn-threads24-I0-D0-R50-U50-RANGEE0-SI2-2022-09-11-15-19-14',
  2000 : 'cheng_normal-Cheng-2000txns-8oppertxn-threads24-I0-D0-R50-U50-RANGEE0-SI2-2022-09-11-15-20-11',
  3000 : 'cheng_normal-Cheng-3000txns-8oppertxn-threads24-I0-D0-R50-U50-RANGEE0-SI2-2022-09-11-15-21-34',
  4000 : 'cheng_normal-Cheng-4000txns-8oppertxn-threads24-I0-D0-R50-U50-RANGEE0-SI2-2022-09-11-15-23-21',
  5000 : 'cheng_normal-Cheng-5000txns-8oppertxn-threads24-I0-D0-R50-U50-RANGEE0-SI2-2022-09-11-15-25-36',
  6000 : 'cheng_normal-Cheng-6000txns-8oppertxn-threads24-I0-D0-R50-U50-RANGEE0-SI2-2022-09-11-15-28-14',
  7000 : 'cheng_normal-Cheng-7000txns-8oppertxn-threads24-I0-D0-R50-U50-RANGEE0-SI2-2022-09-11-15-31-22',
  8000 : 'cheng_normal-Cheng-8000txns-8oppertxn-threads24-I0-D0-R50-U50-RANGEE0-SI2-2022-09-11-15-34-53',
  9000 : 'cheng_normal-Cheng-9000txns-8oppertxn-threads24-I0-D0-R50-U50-RANGEE0-SI2-2022-09-11-15-38-48',
  10000: 'cheng_normal-Cheng-10000txns-8oppertxn-threads24-I0-D0-R50-U50-RANGEE0-SI2-2022-09-11-15-43-10',
}

def run_single(history_dir):
  print('--- checking {} ---'.format(history_dir))
  bincode_path = os.path.join(history_path, history_dir, 'log')
  time = 0
  if checker == 'polysi':
    logs = subprocess.run(['java', '-jar', checker_path, 'audit', '--type={}'.format(history_type), bincode_path], capture_output=True, text=True).stderr.split(os.linesep)
    for log in logs:
      if log == '':
        continue
      if log.find(':') == -1:
        if log[0] == '[':
          assert log == '[[[[ ACCEPT ]]]]' # must satisfy history
        continue
      arg_name, arg = log.split(':')[0].strip(), log.split(':')[-1].strip()
      if arg_name == 'ENTIRE_EXPERIMENT':
        time += int(arg[:-2]) # xxx'ms'
  elif checker == 'nusi':
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
        assert log.split(':')[-1].strip() == 'true' # must satisfy si
  return time

all_statistics = {}
for n_txns in history_names.keys():
  all_statistics[n_txns] = run_single(history_names[n_txns])
for n_txns in all_statistics.keys():
  runtime = all_statistics[n_txns]
  print(f'({n_txns},{runtime})')
