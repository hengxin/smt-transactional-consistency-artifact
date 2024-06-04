from logging import config
import os
import subprocess
import shutil # pip3 install pytest-shutil
from rich.progress import track


# === config ===

n_hist = '3'
histories_to_be_added = [
  '20_250_10_8000_0.5_r_0.5_100',
  '20_500_10_8000_0.5_r_0.5_100',
  '20_750_10_8000_0.5_r_0.5_100',
  '20_800_10_8000_0.5_r_0.5_100',
  '20_900_10_8000_0.5_r_0.5_100',
  '20_1000_10_8000_0.5_r_0.5_100',
]

dbcop = '/home/rikka/dbcop-plus/target/release/dbcop'

# under history/${specific-logs}/${history_name}/hist-00000/history.bincode
root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
specific_path = 'general-list-append/same-listappend-rw-big'
history_dir = os.path.join(root_path, 'history', 'ser', specific_path)
# history_dir = os.path.join(root_path, 'history', 'si', specific_path)

# === main thread ===
gen_dir = '/tmp/gen'
hist_dir = '/tmp/hist'
if not os.path.exists(gen_dir):
  os.mkdir(gen_dir)
if not os.path.exists(hist_dir):
  os.mkdir(hist_dir)
if len(os.listdir(gen_dir)) != 0:
  print(f'{gen_dir} is not empty, clear it')
  shutil.rmtree(gen_dir)
if len(os.listdir(hist_dir)) != 0:
  print(f'{hist_dir} is not empty, clear it')
  shutil.rmtree(hist_dir)

for history in histories_to_be_added:
  specific_path = os.path.join(history_dir, history)
  if os.path.exists(specific_path):
    print(f'path {specific_path} is already existed, skip')
    continue

  configs = [_.strip() for _ in history.split('_')]
  if len(configs) != 8:
    print(f'can NOT parse invalid history config {history}, skip')
    continue
  n_sess, n_txns, n_evts, n_keys, read_p, r, zipf_s, zipf_N = configs
  print(f'gen {history}')
  if r == 'r': # repeat value
    cmd = [dbcop, 'generate', '-d', '/tmp/gen', 
                  '-n', n_sess,
                  '-t', n_txns,
                  '-e', n_evts,
                  '-v', n_keys,
                  '--readp', read_p,
                  '--key_distrib', 'zipf',
                  '--repeat_value',
                  '-s', zipf_s,
                  '-N', zipf_N,
                  '--nhist', n_hist]
  else: # unique value
    cmd = [dbcop, 'generate', '-d', '/tmp/gen', 
                  '-n', n_sess,
                  '-t', n_txns,
                  '-e', n_evts,
                  '-v', n_keys,
                  '--readp', read_p,
                  '--key_distrib', 'zipf',
                  '--nhist', n_hist]
  subprocess.run(cmd) 
  # will gen hist-00000.bincode under /tmp/gen
  os.mkdir(specific_path)
  for i in range(int(n_hist)):
    assert i < 10000
    name = f'hist-{i:05d}.bincode'
    shutil.copy(f'/tmp/gen/{name}', os.path.join(specific_path, name))

  subprocess.run([dbcop, 'run', '-d', '/tmp/gen/', '--db', 'postgres-ser', '-o', '/tmp/hist', '127.0.0.1:5432'])
  # will gen hist-00000/history.bincode under /tmp/hist
  
  # os.mkdir(specific_path)
  for i in range(int(n_hist)):
    assert i < 10000
    name = f'hist-{i:05d}'
    shutil.move(f'/tmp/hist/{name}', os.path.join(specific_path, name))
  
  


