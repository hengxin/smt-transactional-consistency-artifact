import json
import os
import sys

# SER
data_name = 'roachdb_general_all_writes'
# data_name = 'roachdb_general_partition_writes'
# data_name = 'roachdb_all_writes'
# data_name = 'roachdb_partition_writes'

# SI
# data_name = 'galera_partition_writes'
# data_name = 'galera_all_writes'

# checker = 'ours'
# checker = 'polysi'
checker = 'cobra'
# checker = 'dbcop'

if checker != 'dbcop':
  root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
  result_path = os.path.join(root_path, 'results')
  if checker == 'ours':
    data_path = os.path.join(result_path, data_name + '.json')
  else:
    data_path = os.path.join(result_path, data_name + f'-{checker}' + '.json')
  print(data_path)

  with open(data_path) as data:
    data = json.load(data)
  
  accept_cnt, reject_cnt = 0, 0 
  accept_runtime, reject_runtime = 0, 0
  for task in data.keys():
    if data[task]['accept']:
      accept_cnt += 1
      accept_runtime += float(data[task]['total time'][:-2])
    else:
      assert not data[task]['accept']
      reject_cnt += 1
      reject_runtime += float(data[task]['total time'][:-2])
  
  print(f'checker = {checker}, data name = {data_name}') 
  print(f'accept cnt = {accept_cnt}, reject cnt = {reject_cnt}')
  print(f'accept runtime = {accept_runtime}ms, reject runtime = {reject_runtime}ms')

if checker == 'dbcop':
  dbcop_data_path = f'/home/rikka/dbcop-verifier/output/{data_name}'
  dbcop_results, dbcop_runtimes = {}, {}
  for hist in os.listdir(dbcop_data_path):
    dbcop_hist_path = os.path.join(dbcop_data_path, hist)
    for spec_hist in os.listdir(dbcop_hist_path):
      name = hist + ';' + spec_hist
      json_path = os.path.join(dbcop_hist_path, spec_hist, 'result_log.json')
      with open(json_path, 'r') as result_data:
        content = result_data.read()
        json_content = '{' + content.split('{')[-1]
        json_data = json.loads(json_content)
        if json_data['duration']:
          dbcop_runtimes[name] = json_data['duration']
        if json_data['minViolation'] == 'ok':
          dbcop_results[name] = True
        # elif json_data['minViolation'] == 'Serializable' or json_data['minViolation'] == 'SnapshotIsolation':
        #   dbcop_data[name] = False
        else:
          dbcop_results[name] = False

  accept_cnt, reject_cnt = 0, 0 
  accept_runtime, reject_runtime = 0, 0
  for task in dbcop_results.keys():
    assert task in dbcop_runtimes.keys()
    if dbcop_results[task]:
      accept_cnt += 1
      accept_runtime += float(dbcop_runtimes[task])
    else:
      assert not dbcop_results[task]
      reject_cnt += 1
      reject_runtime += float(dbcop_runtimes[task])
  
  print(f'checker = {checker}, data name = {data_name}') 
  print(f'accept cnt = {accept_cnt}, reject cnt = {reject_cnt}')
  print(f'accept runtime = {accept_runtime * 1000}ms, reject runtime = {reject_runtime * 1000}ms')
