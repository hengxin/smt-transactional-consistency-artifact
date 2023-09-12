import os
import subprocess
from prettytable import PrettyTable

root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
history_path = os.path.join(root_path, 'history')
checker_path = os.path.join(root_path, 'builddir', 'checker')
table = PrettyTable()
table.field_names = ['name', '#rw on same key txns / #txns', 'avg written txns on per key / #visited keys', '#<= 2 write keys']
for history_dir in os.listdir(history_path):
  print('[checking] in ./history/{}/'.format(history_dir))
  bincode_path = os.path.join(history_path, history_dir, 'hist-00000', 'history.bincode')
  result = subprocess.run([checker_path, bincode_path], capture_output=True, text=True)
  logs = result.stdout.split(os.linesep)
  table_line = [history_dir]
  for log in logs:
    if log == '':
      continue
    if (log[0] == '['):
      continue
    # if log[0] == 'a': # 'accept: true / false'
    #   table_line.append(log.split(':')[-1].strip())
    # else: # '[time] [addr] [log_level] #...:...'
    #   table_line.append(log.split('#')[-1].split(':')[-1].strip())
    table_line.append(log)
  while len(table_line) < len(table.field_names):
    table_line.append('')
  if len(table_line) == len(table.field_names):
    table.add_row(table_line)
  else:
    print('Parse failed, name = {}'.format(history_dir))
print(table) 