from logging import config
import os
from shlex import join
import subprocess
import shutil # pip3 install pytest-shutil
from rich.progress import track


# === config ===
histories_to_be_added = [
  '20_250_15_1000_0.5',
  '20_250_15_2000_0.5',
  '20_250_15_3000_0.5',
  '20_250_15_4000_0.5',
  '20_250_15_5000_0.5',
  '20_250_15_6000_0.5',
  '20_250_15_7000_0.5',
  '20_250_15_8000_0.5',
  '20_250_15_9000_0.5',
  '20_250_15_10000_0.5',
]

dbcop = '/home/rikka/dbcop/target/release/dbcop'

# under history/${specific-logs}/${history_name}/hist-00000/history.bincode
root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
specific_path = 'dbcop-logs/no-uv/polysi-fig7-like'
# specific_path = 'dbcop-logs/tmp'
history_dir = os.path.join(root_path, 'history', specific_path)

# === main thread ===
gen_dir = '/tmp/gen'
hist_dir = '/tmp/hist'
if len(os.listdir(gen_dir)) != 0:
  print(f'{gen_dir} is not empty, clear it')
  shutil.rmtree(gen_dir)
if len(os.listdir(hist_dir)) != 0:
  print(f'{hist_dir} is not empty, clear it')
  shutil.rmtree(hist_dir)

for history in track(histories_to_be_added):
  configs = [_.strip() for _ in history.split('_')]
  if len(configs) != 5:
    print(f'can NOT parse invalid history config {history}, skip')
    continue
  n_sess, n_txns, n_evts, n_keys, rep_rate = configs

  print(f'gen {history}')
  subprocess.run([dbcop, 'generate', '-d', '/tmp/gen', 
                  '-n', n_sess,
                  '-t', n_txns,
                  '-e', n_evts,
                  '-v', n_keys,
                  '-r', rep_rate]) 
  # will gen hist-00000.bincode under /tmp/gen
  subprocess.run([dbcop, 'run', '-d', '/tmp/gen/', '--db', 'postgres-ser', '-o', '/tmp/hist', '127.0.0.1:5432'])
  # will gen hist-00000/history.bincode under /tmp/hist
  
  specific_path = os.path.join(history_dir, history)
  print(specific_path)
  if os.path.exists(specific_path):
    print(f'path {specific_path} is already existed, skip')
  else:
    os.mkdir(specific_path)
  shutil.move('/tmp/hist/hist-00000', os.path.join(specific_path, 'hist-00000'))
  
  


