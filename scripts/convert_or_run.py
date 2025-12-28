from logging import config
import os
import subprocess
import shutil # pip3 install pytest-shutil
from rich.progress import track


# === config ===

dbcop = '/home/rikka/dbcop-plus/target/release/dbcop'

# under history/${specific-logs}/${history_name}/hist-00000/history.bincode
root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
# specific_path = 'general-list-append/list-rw'
specific_path = 'general-list-append/list-rw-various'
history_dir = os.path.join(root_path, 'history', 'ser', specific_path)
# history_dir = os.path.join(root_path, 'history', 'si', specific_path)

# === main thread ===
for history in os.listdir(history_dir):
  specific_path = os.path.join(history_dir, history)
  for specified_history in os.listdir(specific_path):
    if specified_history.endswith(".bincode"):
      continue
    cmd = [dbcop, "convert", "-d", os.path.join(specific_path, specified_history), "--from", "bincode"]
    subprocess.run(cmd)  
    # cmd_str = f"lein run test --case-workload {os.path.join(specific_path, specified_history)}/history.json --no-ssh --node localhost"
    # print(cmd_str)
  


