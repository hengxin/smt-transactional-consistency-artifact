import json
import os
import sys

data_name = 'all'
# data_name = 'partition'

# 1. load NuSer data
root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
result_path = os.path.join(root_path, 'results')
data_path = os.path.join(result_path, f'oopsla19-base-fp-pc-wrcp-roachdb-{data_name}-writes' + '.json')

with open(data_path) as data:
  data = json.load(data)

# 2. load dbcop data
dbcop_data_path = f'/home/rikka/dbcop+/dbcop-verifier/results/correctness/roachdb_general_{data_name}_writes'
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
      if json_data['minViolation'] == 'ok':
        dbcop_data[name] = True
      elif json_data['minViolation'] == 'Serializable':
        dbcop_data[name] = False
      else:
        assert(0)
      # print(json_data)
      # sys.exit(0)

# 3. compare data and dbcop_data
assert len(data.keys()) == len(dbcop_data.keys())
for task in data.keys():
  assert task in dbcop_data.keys()
  assert data[task]['accept'] == dbcop_data[task]

print('All Same, Ok!')