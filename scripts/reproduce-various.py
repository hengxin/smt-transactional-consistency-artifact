# reproduce time / memory expense

import os
import pandas
import json
import time
import sys

# === config ===
inf = 0x7fffffffffffffff
participants = [
  # {
  #   'name': 'ours',
  #   'data_path': 'various-ours'
  # },
  # {
  #   'name': 'ours-mono',
  #   'data_path': 'various-ours-mono'
  # },
  # {
  #   'name': 'ours-PK',
  #   'data_path': 'various-ours-PK'
  # },
  # {
  #   'name': 'reduce-kg',
  #   'data_path': 'various+reduce'
  # },
  # {
  #   'name': 'no-reduce',
  #   'data_path': 'various+no-reduce'
  # },
  {
    'name': 'baseline',
    'data_path': 'new-opt/various-baseline'
  },
  # {
  #   'name': 'mono',
  #   'data_path': 'various+ours-mono'
  # },
  {
    'name': 'ours',
    'data_path': 'various-ours'
  },
  # {
  #   'name': 'ours-HT',
  #   'data_path': 'various+HT'
  # },
  # {
  #   'name': 'ours-HT-HD',
  #   'data_path': 'various+HT-HD'
  # },
  # {
  #   'name': 'ours-HT-HD-R',
  #   'data_path': 'various+HT-HD-R'
  # },
]

root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
result_path = os.path.join(root_path, 'results')

experiment_set = [
  {
    'id': 'a',
    'name': 'sessions', # ${name}.json is the output file
    'set': '{}_100_15_5000_0.5_r_0.5_100',
    'param': [5, 10, 15, 20, 25, 30],
  },
  {
    'id': 'b',
    'name': 'txns',
    'set': '20_{}_15_5000_0.5_r_0.5_100',
    'param': [10, 20, 30, 40, 50, 100, 150, 200, 250],
  },
  {
    'id': 'c',
    'name': 'evts',
    'set': '20_100_{}_5000_0.5_r_0.5_100',
    'param': [5, 10, 15, 20, 25, 30],
  },
  {
    'id': 'd',
    'name': 'readpct', 
    'set': '20_100_15_5000_{}_r_0.5_100',
    'param': [0.05, 0.25, 0.5, 0.75, 0.95],
  },
  # {
  #   'id': 'e',
  #   'name': 'zipf_s',
  #   'set': '20_100_15_5000_0.5_r_{}_100',
  #   'param': [0, 0.5, 1, 1.5, 2, 2.5, 4],
  # },
]

def adjust_time(time): # ms -> s
  if isinstance(time, str):
    if time.endswith('ms'):
      time = time[:-2]
    return int(time) / 1000.0 
  if isinstance(time, (int, float)):
    return time / 1000.0


def adjust_memory(memory):
  if isinstance(memory, str):
    memory = int(memory)
  if isinstance(memory, (int, float)):
    return memory / 1024.0 / 1024.0


def to_int(s):
  if s == 'TO':
    return inf
  if isinstance(s, int):
    return s
  if s.endswith('ms'):
    s = s[:-2]
  return int(s)


def get_avg_data(data, hist_name, field_name):
  ret = [to_int(data[name][field_name]) for name in data.keys() if name.startswith(hist_name)]
  assert len(ret) > 0
  return sum(ret) / len(ret)


if __name__ == '__main__':
  # load data from .json
  for participant in participants:
    with open(os.path.join(result_path, participant['data_path'] + '.json')) as data:
      participant['data'] = json.load(data)
  
  # generate unique result folder
  output_dir = 'data-' + str(time.time())
  output_dir_path = os.path.join(result_path, output_dir)
  if os.path.exists(output_dir_path):
    print(f'path {output_dir} is existed, exit')
    sys.exit(1)
  os.mkdir(output_dir_path)
  
  for sub_exp in experiment_set:
    runtime_df = pandas.DataFrame()
    runtime_df['param'] = sub_exp['param'] if not sub_exp['name'].endswith('pct') else [int(_ * 100) for _ in sub_exp['param']]
    
    memory_df = pandas.DataFrame()
    memory_df['param'] = sub_exp['param'] if not sub_exp['name'].endswith('pct') else [int(_ * 100) for _ in sub_exp['param']]
    
    for participant in participants:
      # data = [adjust_time(participant['data'][sub_exp['set'].format(_)]['total time']) for _ in sub_exp['param']]
      data = [adjust_time(get_avg_data(participant['data'], sub_exp['set'].format(_), 'total time')) for _ in sub_exp['param']]
      runtime_df[participant['name']] = data

      # data = [adjust_memory(participant['data'][sub_exp['set'].format(_)]['max memory']) for _ in sub_exp['param']]
      data = [adjust_memory(get_avg_data(participant['data'], sub_exp['set'].format(_), 'max memory')) for _ in sub_exp['param']]
      memory_df[participant['name']] = data
    
    runtime_df.to_csv(os.path.join(output_dir_path, 'runtime-' + sub_exp['name'] + '.csv'), index=False)
    print('gen {}'.format('runtime-' + sub_exp['name'] + '.csv'))
    
    memory_df.to_csv(os.path.join(output_dir_path, 'mem-' + sub_exp['name'] + '.csv'), index=False)
    print('gen {}'.format('mem-' + sub_exp['name'] + '.csv'))
         



  