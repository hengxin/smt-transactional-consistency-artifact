from logging import config
import os
import subprocess
import shutil # pip3 install pytest-shutil
from rich.progress import track


# === config ===

n_hist = '1'
histories_to_be_added = [
  # Memory
  # '10_25_8_50_0.5_r_0.5_120',
  # '10_50_8_100_0.5_r_0.5_120',
  # '10_200_8_100_0.5_r_0.5_120',
  # '10_100_8_100_0.5_r_0.5_120',
  '10_10_8_50_0.5_r_0.5_130',
  '10_20_8_50_0.5_r_0.5_130',
  '10_30_8_50_0.5_r_0.5_130',
  '10_40_8_50_0.5_r_0.5_130',
  '10_50_8_50_0.5_r_0.5_130',

  # General
  # '25_400_8_5000_0.3_r_0.5_100',
  # '25_400_8_5000_0.5_r_0.5_100',
  # '25_400_8_5000_0.95_r_0.5_100',
  # '25_400_8_5000_0.5_r_0.75_10',
  # '50_400_8_10000_0.5_r_0.75_10',

  # Various
  # default: 20_100_15_5000_0.5_r_0.5_100
  # 1. sess
  # '5_100_15_5000_0.5_r_0.5_100',
  # '10_100_15_5000_0.5_r_0.5_100',
  # '15_100_15_5000_0.5_r_0.5_100',
  # '20_100_15_5000_0.5_r_0.5_100',
  # '25_100_15_5000_0.5_r_0.5_100',
  # '30_100_15_5000_0.5_r_0.5_100',
  # '40_100_15_5000_0.5_r_0.5_100',
  # '50_100_15_5000_0.5_r_0.5_100',
  # 2. txn
  # '20_10_15_5000_0.5_r_0.5_100',
  # '20_20_15_5000_0.5_r_0.5_100',
  # '20_30_15_5000_0.5_r_0.5_100',
  # '20_40_15_5000_0.5_r_0.5_100',
  # '20_50_15_5000_0.5_r_0.5_100',
  # '20_100_15_5000_0.5_r_0.5_100',
  # '20_150_15_5000_0.5_r_0.5_100',
  # '20_200_15_5000_0.5_r_0.5_100',
  # '20_250_15_5000_0.5_r_0.5_100',
  # '20_300_15_5000_0.5_r_0.5_100',
  # '20_400_15_5000_0.5_r_0.5_100',
  # '20_500_15_5000_0.5_r_0.5_100',
  # 3. ops
  # '20_100_5_5000_0.5_r_0.5_100',
  # '20_100_10_5000_0.5_r_0.5_100',
  # '20_100_15_5000_0.5_r_0.5_100',
  # '20_100_20_5000_0.5_r_0.5_100',
  # '20_100_25_5000_0.5_r_0.5_100',
  # '20_100_30_5000_0.5_r_0.5_100',
  # 4. readp
  # '20_100_15_5000_0.01_r_0.5_100',
  # '20_100_15_5000_0.02_r_0.5_100',
  # '20_100_15_5000_0.03_r_0.5_100',
  # '20_100_15_5000_0.04_r_0.5_100',
  # '20_100_15_5000_0.05_r_0.5_100',
  # '20_100_15_5000_0.25_r_0.5_100',
  # '20_100_15_5000_0.5_r_0.5_100',
  # '20_100_15_5000_0.75_r_0.5_100',
  # '20_100_15_5000_0.95_r_0.5_100',
  # 5. keys, skip it
  # '20_100_15_1000_0.5_r_0.5_100',
  # '20_100_15_2000_0.5_r_0.5_100',
  # '20_100_15_3000_0.5_r_0.5_100',
  # '20_100_15_4000_0.5_r_0.5_100',
  # '20_100_15_5000_0.5_r_0.5_100',
  # '20_100_15_6000_0.5_r_0.5_100',
  # '20_100_15_7000_0.5_r_0.5_100',
  # '20_100_15_8000_0.5_r_0.5_100',
  # '20_100_15_9000_0.5_r_0.5_100',
  # '20_100_15_10000_0.5_r_0.5_100',
  # 6. zipf s
  # '20_100_15_5000_0.5_r_0_100',
  # '20_100_15_5000_0.5_r_0.5_100',
  # '20_100_15_5000_0.5_r_1_100',
  # '20_100_15_5000_0.5_r_1.5_100',
  # '20_100_15_5000_0.5_r_2_100',
  # '20_100_15_5000_0.5_r_2.5_100',
  # '20_100_15_5000_0.5_r_3_100',
  # '20_100_15_5000_0.5_r_4_100',
]

dbcop = '/home/rikka/dbcop-plus/target/release/dbcop'

# under history/${specific-logs}/${history_name}/hist-00000/history.bincode
root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
# specific_path = 'dbcop-logs/no-uv/scalability4'
specific_path = 'dbcop-logs/tmp2'
# specific_path = 'dbcop-logs/various'
history_dir = os.path.join(root_path, 'history', 'ser', specific_path)

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
                  '--key_distrib', 'uniform',
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
                  '--nhist', n_hist]
  subprocess.run(cmd) 
  # will gen hist-00000.bincode under /tmp/gen
  subprocess.run([dbcop, 'run', '-d', '/tmp/gen/', '--db', 'postgres-ser', '-o', '/tmp/hist', '127.0.0.1:5432'])
  # will gen hist-00000/history.bincode under /tmp/hist
  
  os.mkdir(specific_path)
  for i in range(int(n_hist)):
    assert i < 10
    shutil.move(f'/tmp/hist/hist-0000{i}', os.path.join(specific_path, f'hist-0000{i}'))
  
  


