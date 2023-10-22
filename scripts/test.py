import os
import subprocess
from prettytable import PrettyTable

history_type = 'dbcop'
root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
# history_path = os.path.join(root_path, 'history', '{}-logs'.format(history_type), 'one-shot-chengRW')
history_path = os.path.join(root_path, 'history', '{}-logs'.format(history_type))
print(history_path)
checker_path = os.path.join(root_path, 'builddir', 'checker')
table = PrettyTable()
table.field_names = ['name', '#sessions', '#txns', '#events', '#constrains', 'construct time', 'prune time', 'init time', 'solve time', 'status']
solver = 'acyclic-minisat'
print('[use] {} as backend solver'.format(solver))
for history_dir in os.listdir(history_path):
  # if len(history_dir) >= 14 or (history_dir[0] > '9' or history_dir[0] < '0'):
  #   print('[skip] ./history/{}/'.format(history_dir))
  #   continue
  print('[running checker] in ./history/{}/'.format(history_dir))
  if history_type == 'cobra':
    bincode_path = str(os.path.join(history_path, history_dir)) + '/'
    result = subprocess.run([checker_path, bincode_path, '--solver', solver, '--history-type', history_type, '--pruning'], capture_output=True, text=True)
  else: # dbcop
    bincode_path = os.path.join(history_path, history_dir, 'hist-00000', 'history.bincode')
    result = subprocess.run([checker_path, bincode_path, '--solver', solver, '--history-type', history_type, '--pruning'], capture_output=True, text=True)

  logs = result.stdout.split(os.linesep)
  table_line = [history_dir]
  for log in logs:
    if log == '':
      continue
    # if log[0] == 'a': # 'accept: true / false'
    #   table_line.append(log.split(':')[-1].strip())
    # else: # '[time] [addr] [log_level] #...:...'
    #   table_line.append(log.split('#')[-1].split(':')[-1].strip())
    if log[0] == '[':
      if log.find(':') == -1:
        continue
      table_line.append(log.split(':')[-1].strip())
    else:
      if log[0] == 'a': # accept
        table_line.append('accept {}'.format(log.split(':')[-1].strip()))
      elif log[0] == 'K': # Killed
        while len(table_line) < len(table.field_names) - 1:
          table_line.append('')
        table_line.append('killed')
      else:
        print('Unknown log:', end=' ')
        print(log)
        continue
  while len(table_line) < len(table.field_names):
    table_line.append('')
  if len(table_line) == len(table.field_names):
    table.add_row(table_line)
  else:
    print('Parse failed, name = {}'.format(history_dir))
print(table) 