import json
import os

root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
result_path = os.path.join(root_path, 'results')
data_path = os.path.join(result_path, 'oopsla19-base-fp-pc-wrcp-roachdb-all-writes' + '.json')

with open(data_path) as data:
  data = json.load(data)

accept_cnt = 0
unaccept_cnt = 0
for hist in data.keys():
  if data[hist]['accept']:
    accept_cnt += 1
  else:
    unaccept_cnt += 1

print(f'accept cnt = {accept_cnt}')
print(f'unaccept cnt = {unaccept_cnt}')