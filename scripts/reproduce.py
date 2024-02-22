# reproduce time / memory expense

import os
import pandas
import json
import time
import sys

# === config ===
participants = [
  {
    'name': 'mono',
    'data_path': 'pf7-mono-prune'
  },
  {
    'name': 'am',
    'data_path': 'pf7-am-prune-cache-key-known-mem'
  }, 
]

root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
result_path = os.path.join(root_path, 'results')

experiment_set = [
  {
    'id': 'a',
    'name': 'sessions', # ${name}.json is the output file
    'set': '{}_200_15_2000_0.5',
    'param': [5, 10, 15, 20, 25, 30],
  },
  {
    'id': 'b',
    'name': 'txns',
    'set': '20_{}_15_2000_0.5',
    'param': [10, 20, 30, 40, 50, 100, 150, 200, 250],
  },
  {
    'id': 'c',
    'name': 'evts',
    'set': '20_100_{}_2000_0.5',
    'param': [5, 10, 15, 20, 25, 30],
  },
  {
    'id': 'd',
    'name': 'readpct', 
    'set': '10_50_15_1000_0.5_{}',
    'param': [0, 0.25, 0.5, 0.75, 1],
  },
  {
    'id': 'e',
    'name': 'keys',
    'set': '20_250_15_{}_0.5',
    'param': [1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000],
  },
  {
    'id': 'f',
    'name': 'repeatpct',
    'set': '10_50_20_1000_{}',
    'param': [0, 0.2, 0.4, 0.6, 0.8, 1],
  }
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
      data = [adjust_time(participant['data'][sub_exp['set'].format(_)]['total time']) for _ in sub_exp['param']]
      runtime_df[participant['name']] = data

      data = [adjust_memory(participant['data'][sub_exp['set'].format(_)]['max memory']) for _ in sub_exp['param']]
      memory_df[participant['name']] = data
    
    runtime_df.to_csv(os.path.join(output_dir_path, 'runtime-' + sub_exp['name'] + '.csv'), index=False)
    print('gen {}'.format('runtime-' + sub_exp['name'] + '.csv'))
    
    memory_df.to_csv(os.path.join(output_dir_path, 'mem-' + sub_exp['name'] + '.csv'), index=False)
    print('gen {}'.format('mem-' + sub_exp['name'] + '.csv'))
         



  