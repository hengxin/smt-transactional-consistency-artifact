import os
import subprocess
from prettytable import PrettyTable

root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
history_path = os.path.join(root_path, 'history')
checker_path = os.path.join(root_path, 'builddir', 'checker')
table = PrettyTable()
table.field_names = ['name', 'sessions', 'txns', 'events', 'constrains', 'construct time', 'solve time', 'accept']
for history_dir in os.listdir(history_path):
  print('[log] checking in ./history/{}/'.format(history_dir))
  bincode_path = os.path.join(history_path, history_dir, 'hist-00000', 'history.bincode')
  result = subprocess.run([checker_path, bincode_path, '--pruning'], capture_output=True, text=True)
  logs = result.stdout.split(os.linesep)
  table_line = [history_dir]
  for log in logs:
    if log == '':
      continue
    if log[0] == 'a': # 'accept: true / false'
      table_line.append(log.split(':')[-1].strip())
    else: # '[time] [addr] [log_level] #...:...'
      table_line.append(log.split('#')[-1].split(':')[-1].strip())
  if len(table_line) != len(table.field_names): # Killed, seems terminated by z3 itself, but why? 
    continue
  table.add_row(table_line)
print(table) 