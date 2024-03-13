# reproduce time / memory expense

import os
import pandas
import json
import time
import sys

# === config ===
participants = [
  {
    'name': 'NuSer',
    'data_path': 'uvd-NuSer-926'
  },
  {
    'name': 'SERSolver',
    'data_path': 'uvd-SERSolver-926'
  },
  {
    'name': 'dbcop',
    'data_path': 'uvd-dbcop-926'
  },
]

root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
result_path = os.path.join(root_path, 'results')

experiment_set = [ # default 15_45_15_1000 
  {
    'id': 'a',
    'name': 'sessions', # ${name}.json is the output file
    'set': '{}_45_15_1000',
    'param': [5, 10, 15, 20, 25],
  },
  {
    'id': 'b',
    'name': 'txns',
    'set': '15_{}_15_1000',
    'param': [15, 30, 45, 60, 75],
  },
  {
    'id': 'c',
    'name': 'evts',
    'set': '15_45_{}_1000',
    'param': [5, 10, 15, 20, 25],
  },
  {
    'id': 'd',
    'name': 'keys',
    'set': '15_45_15_{}',
    'param': [500, 750, 1000, 1250, 1500],
  },
  {
    'id': 'e',
    'name': 'txns-stress',
    'set': '15_{}_15_1000',
    'param': [100, 200, 300, 400, 500, 600, 700],
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
         



  