import json
import os
import sys

# data_name = 'roachdb_general_all_writes'
# data_name = 'roachdb_general_partition_writes'
# data_name = 'galera_partition_writes'
# data_name = 'galera_all_writes'

# data_name = 'roachdb_all_writes'
data_name = 'roachdb_partition_writes'

# checker = 'polysi'
checker = 'ours'
# checker = 'cobra'

# 1. load NuSer data
root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
result_path = os.path.join(root_path, 'results')
if checker == 'ours':
  data_path = os.path.join(result_path, data_name + '.json')
else:
  data_path = os.path.join(result_path, data_name + f'-{checker}' + '.json')
  print(data_path)

with open(data_path) as data:
  data = json.load(data)

# 2. load dbcop data
dbcop_data_path = f'/home/rikka/dbcop-verifier/output/{data_name}'
dbcop_data = {}
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
        dbcop_data[name] = json_data['duration']
      # print(json_data)
      # sys.exit(0)

# 3. compare data and dbcop_data
# assert len(data.keys()) == len(dbcop_data.keys())
ours_runtime, dbcop_runtime = 0, 0
for task in data.keys():
  assert task in dbcop_data.keys()
  ours_runtime += float(data[task]['total time'][:-2])
  dbcop_runtime += dbcop_data[task]
  

print(f'ours runtime = {ours_runtime}ms, dbcop runtime = {dbcop_runtime * 1000}ms')