# specifically used for dbcop oopsla19 dataset 
# === imports ===
# system
import os
import subprocess
import humanize
import psutil
import time
import resource
import shutil

# progress bar
from rich.progress import (
  Progress,
  TimeElapsedColumn,
  TextColumn, 
  BarColumn, 
  SpinnerColumn
) 
from rich.console import Group
from rich.panel import Panel
from rich.live import Live

# multithread
import threading
import queue

# logger
import logging

# serialize results
import json

# output results
from rich.console import Console
from rich.table import Table

# === config ===

def set_memory_limit(memory_in_kb):
  soft, hard = resource.getrlimit(resource.RLIMIT_AS)
  resource.setrlimit(resource.RLIMIT_AS, (memory_in_kb, hard))

# 设置虚拟内存限制为 1GB
# set_memory_limit(10 * 1024 * 1024) # 10 GB

logging.basicConfig(
  level = logging.DEBUG,  
  format = '[%(asctime)s] [%(levelname)s]  %(message)s',  
  filename = 'test.log',  
  filemode = 'w'  
)

checker = 'cobra'
assert checker == 'polysi' or checker == 'viper' or checker == 'ours' or checker == 'cobra'
logging.info(f'checker = {checker}')

history_type = 'dbcop' 
assert history_type == 'cobra' or history_type == 'dbcop'
logging.info(f'history type = {history_type}')

root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
logging.info(f'root path = {root_path}')
# will run all histories under this path
# history_path = os.path.join(root_path, 'history', 'ser', '{}-logs'.format(history_type), 'various')
# history_path = os.path.join(root_path, 'history', 'ser', 'same-listappend-rw')
# history_path = os.path.join(root_path, 'history', 'ser', 'various-pldi')
# history_path = os.path.join(root_path, 'history', 'ser', '{}-logs'.format(history_type), 'oopsla19', 'roachdb_all_writes')
# history_path = os.path.join(root_path, 'history', 'ser', '{}-logs'.format(history_type), 'oopsla19', 'roachdb_partition_writes')
# history_path = os.path.join(root_path, 'history', 'ser', '{}-logs'.format(history_type), 'oopsla19', 'roachdb_general_partition_writes')
history_path = os.path.join(root_path, 'history', 'ser', '{}-logs'.format(history_type), 'oopsla19', 'roachdb_general_all_writes')
# history_path = os.path.join(root_path, 'history', 'ser', '{}-logs'.format(history_type), 'oopsla19', 'galera_all_writes')
# history_path = os.path.join(root_path, 'history', 'ser', '{}-logs'.format(history_type), 'oopsla19', 'galera_partition_writes')
logging.info(f'history path = {history_path}')

if checker == 'ours':
  # checker_path = os.path.join(root_path, 'builddir', 'checker')
  checker_path = os.path.join(root_path, 'builddir-release', 'checker')
elif checker == 'polysi':
  checker_path = '/home/rikka/PolySI/build/libs/PolySI-1.0.0-SNAPSHOT.jar'
elif checker == 'viper':
  checker_path = '/home/rikka/Viper/src/main_allcases.py'
  config_path = '/home/rikka/Viper/src/config.yaml'
elif checker == 'cobra':
  cobra_path = '/home/rikka/PolySI-PVLDB2023-Artifacts/artifact/CobraVerifier'
  checker_path = os.path.join(cobra_path, 'target', 'CobraVerifier-0.0.1-SNAPSHOT-jar-with-dependencies.jar')
  config_path = '/tmp/cobra.conf.nogpu'
logging.info(f'checker path = {checker_path}')

solver = 'acyclic-minisat'
# solver = 'monosat'
assert solver == 'acyclic-minisat' or solver == 'monosat' or solver == 'z3' or solver == 'monosat-baseline'
logging.info(f'solver = {solver}')

pruning_method = 'fast'
assert pruning_method == 'fast' or pruning_method == 'normal' or pruning_method == 'none' or pruning_method == 'basic'
logging.info(f'pruning method = {pruning_method}')

isolation_level = 'ser'
assert isolation_level == 'ser' or isolation_level == 'si'
logging.info(f'isolation level = {isolation_level}')

# on 926 ubuntu, it's okay to set n_threads to 4
# on local virtual machine, n_threads is recommanded to be set to 3
# a large n_threads may lead to the not-full-usage of a cpu core, or trigger processes being incorrectly killed due to the exceeded memory usage
n_threads = 1
logging.info(f'use {n_threads} thread(s)')

output_path = os.path.join(root_path, 'results', 'roachdb_general_all_writes-cobra.json')
logging.info(f'output path = {output_path}')

timeout_duration = 60 # s

# === global variables ===
# tasks = os.listdir(history_path)
# task_queue = queue.Queue()
# for history_dir in tasks:
#   task_queue.put(history_dir)
tasks = []
task_queue = queue.Queue()
if history_type == 'cobra':
  for history_dir in tasks:
    task_queue.put(history_dir)
    tasks.append(history_dir)
else: # dbcop
  for history_dir in os.listdir(history_path):
    spec_history_path = os.path.join(history_path, history_dir)
    for spec_history in os.listdir(spec_history_path):
      task_name = history_dir + ';' + spec_history
      task_queue.put(task_name)
      tasks.append(task_name)
      # break

n_finished = 0
n_finished_lock = threading.Lock()

def add_n_finished():
  global n_finished
  with n_finished_lock:
    n_finished += 1

results = { name: {} for name in tasks }
results_lock = threading.Lock()

def update_results(thread_id, name, key, value):
  global results
  with results_lock:
    results[name][key] = value
    logging.debug(f'thread {thread_id} updates results[\'{name}\'][\'{key}\'] to \'{value}\'')

# === progress bar definition ===
current_task_progress = Progress(
  TimeElapsedColumn(),
  TextColumn("{task.description}"),
)

task_steps_progress = Progress(
  TextColumn("  "),
  TimeElapsedColumn(),
  TextColumn("{task.description} "),
  SpinnerColumn("circle")
)

overall_progress = Progress(
  TimeElapsedColumn(),
  BarColumn(),
  TextColumn("{task.description}")
)

progress_group = Group(
  Panel(Group(current_task_progress, task_steps_progress)),
  overall_progress
)

overall_task_id = overall_progress.add_task("", total=len(tasks))

# === sub thread ===

def parse_logs(thread_id, logs, max_memory, task_name):
  """parse logs of a task and store"""
  logging.debug(f'thread {thread_id} starts parsing logs of task {task_name}, max memory = {max_memory}, logs = {logs}')
  update_results(thread_id, task_name, 'max memory', max_memory)
  for log in logs:
    if log == '':
      logging.warning(f'thread {thread_id} found \'\'(empty line) in logs, skip')
      continue
    if log[0] == '[': # [time] [thread] [log_level] msg
      msg = log.split(']')[-1].strip()
      if msg.find(':') == -1:
        logging.debug(f'thread {thread_id} skips msg {msg}')
        continue
      key, value = msg.split(':')
      update_results(thread_id, task_name, key.strip(), value.strip())
    elif log[0] == 'a': # accept : true / false
      update_results(thread_id, task_name, 'accept', log.split(':')[-1].strip() == 'true')
    else:
      logging.error(f'thread {thread_id} cannot parse log line "{log}"')


def update_timeout(thread_id, task_name):
  """update timeout"""
  update_results(thread_id, task_name, 'max memory', 'TO')
  update_results(thread_id, task_name, 'total time', 'TO')
  update_results(thread_id, task_name, 'accept', 'TO')


def read_stream(stream):
  output = ''
  for line in stream:
    line = line.decode().rstrip()
    output += line + os.linesep
  return output
  

def run_task(thread_id, task):
  """run a single task"""
  logging.info(f'thread {thread_id} starts running task {task}')

  current_task_id = current_task_progress.add_task("%s" % task, visible=False)
  current_task_steps_id = task_steps_progress.add_task("[bold blue]Thread %d: Running %s" % (thread_id, task))

  if history_type == 'cobra':
    history_dir = task
    bincode_path = str(os.path.join(history_path, history_dir)) + '/'
  else: # dbcop
    history_dir, spec_history = task.split(';')
    bincode_path = os.path.join(history_path, history_dir, spec_history, 'history.bincode')
  
  if checker == 'cobra':
    cobra_tmp_hist_dir = os.path.join(root_path, 'cobra_tmp_hist_dir')
    shutil.rmtree(cobra_tmp_hist_dir)
    os.makedirs(cobra_tmp_hist_dir)
    subprocess.run(
      ['java', '-jar', '/home/rikka/PolySI/build/libs/PolySI-1.0.0-SNAPSHOT.jar', 
       'convert', '-f=dbcop', '-o=cobra', '-t=identity', 
       bincode_path, cobra_tmp_hist_dir],
      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    
    cmd = ['java',
           f'-Djava.library.path={cobra_path}/include/:{cobra_path}/build/monosat',
           '-jar', f"{cobra_path}/target/CobraVerifier-0.0.1-SNAPSHOT-jar-with-dependencies.jar", 
           'mono', 'audit', config_path, cobra_tmp_hist_dir]
    # print(cmd)
    st_time = time.time()
    logs = subprocess.run(cmd, capture_output=True, text=True).stdout.split(os.linesep)
    ed_time = time.time()
    
    accept = False
    for log in logs:
      if log == '':
        continue
      if log[0] == '[':
        if log.endswith('[[[[ REJECT ]]]]'):
          accept = False
        elif log.endswith('[[[[ ACCEPT ]]]]'):
          accept = True
    runtime = (ed_time - st_time) * 1000

    update_results(thread_id, task, 'accept', accept)
    update_results(thread_id, task, 'total time', f'{runtime}ms')

    # update progress bar
    current_task_progress.stop_task(current_task_id)
    current_task_progress.update(current_task_id, description="[bold green]%s [green]:heavy_check_mark:" % (task, ), visible=False)
    task_steps_progress.stop_task(current_task_steps_id)
    task_steps_progress.update(current_task_steps_id, visible=False)
    
    return
    
  if checker == 'viper':
    cobra_tmp_hist_dir = os.path.join(root_path, 'cobra_tmp_hist_dir')
    shutil.rmtree(cobra_tmp_hist_dir)
    os.makedirs(cobra_tmp_hist_dir)
    subprocess.run(
      ['java', '-jar', '/home/rikka/PolySI/build/libs/PolySI-1.0.0-SNAPSHOT.jar', 
       'convert', '-f=dbcop', '-o=cobra', '-t=identity', 
       bincode_path, cobra_tmp_hist_dir],
      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    cmd = ['python3', checker_path, 
           '--config_file', config_path, 
           '--algo', '6', 
           '--sub_dir', cobra_tmp_hist_dir,
           '--perf_file', './test_pertf.txt',
           '--exp_name', 'test', 
           '--strong-session']
    # print(cmd)
    st_time = time.time()
    logs = subprocess.run(cmd, capture_output=True, text=True).stdout.split(os.linesep)
    ed_time = time.time()
    
    print(logs)
    for log in logs:
      if log == '':
        continue
      if log.find(':') == -1:
        continue
      arg_name, arg = log.split(':')
      if arg_name == 'sat':
        accept = True if arg.strip() == 'True' else False 
    runtime = (ed_time - st_time) * 1000

    update_results(thread_id, task, 'accept', accept)
    update_results(thread_id, task, 'total time', f'{runtime}ms')

    # update progress bar
    current_task_progress.stop_task(current_task_id)
    current_task_progress.update(current_task_id, description="[bold green]%s [green]:heavy_check_mark:" % (task, ), visible=False)
    task_steps_progress.stop_task(current_task_steps_id)
    task_steps_progress.update(current_task_steps_id, visible=False)
    
    return
  
  
  if checker == 'polysi':
    logs = subprocess.run(['java', '-jar', checker_path, 'audit', '--type={}'.format(history_type), bincode_path], capture_output=True, text=True).stderr.split(os.linesep)
    max_memory = 0
    runtime = 0
    for log in logs:
      if log == '':
        continue
      if log.find(':') == -1:
        if log[0] == '[':
          if log == '[[[[ ACCEPT ]]]]':
            accept = True
          elif log == '[[[[ REJECT ]]]]':
            accept = False
        continue
      arg_name, arg = log.split(':')[0].strip(), log.split(':')[-1].strip()
      if arg_name == 'ENTIRE_EXPERIMENT':
        runtime += int(arg[:-2]) # xxx'ms'
      if arg_name == 'Max memory':
        max_memory = arg

    update_results(thread_id, task, 'max memory', max_memory)
    update_results(thread_id, task, 'accept', accept)
    update_results(thread_id, task, 'total time', f'{runtime}ms')

    # update progress bar
    current_task_progress.stop_task(current_task_id)
    current_task_progress.update(current_task_id, description="[bold green]%s [green]:heavy_check_mark:" % (task, ), visible=False)
    task_steps_progress.stop_task(current_task_steps_id)
    task_steps_progress.update(current_task_steps_id, visible=False)
    
    return
  
  cmd = [checker_path, bincode_path, '--solver', solver, '--history-type', history_type, '--isolation-level', isolation_level]
  if pruning_method != 'none':
    cmd.append('--pruning')
    cmd.append(pruning_method)
  logging.debug(f'thread {thread_id} runs cmd {cmd}')
  # result = subprocess.run(cmd, capture_output=True, text=True)
  process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

  process_id = process.pid
  max_memory = 0
  timeout_time = time.time() + timeout_duration
  timeout = False

  while process.poll() is None:
    try:
      process_info = psutil.Process(process_id)
      memory_info = process_info.memory_info()
      memory_usage = memory_info.rss
      max_memory = max(max_memory, memory_usage)

    except psutil.NoSuchProcess:
      break

    if time.time() > timeout_time:
      process.terminate()
      process.wait()
      timeout = True
      logging.info(f'!subprocess of task {task} timed out')
      break
  
  if not timeout:
    return_code = process.wait()
    if return_code != 0:
      logging.info(f'!subprocess of task {task} does not return 0')
    time.sleep(1)

    stdout = read_stream(process.stdout)
    stderr = read_stream(process.stderr)
    
    logging.debug(f'thread finished checking {task}, stdout = {stdout}, stderr = {stderr}')
    logs = stdout.split(os.linesep)
    parse_logs(thread_id, logs, max_memory, task)
    logging.info(f'thread {thread_id} finished task {task}')
  else:
    update_timeout(thread_id, task)

  current_task_progress.stop_task(current_task_id)
  current_task_progress.update(current_task_id, description="[bold green]%s [green]:heavy_check_mark:" % (task, ), visible=False)
  task_steps_progress.stop_task(current_task_steps_id)
  task_steps_progress.update(current_task_steps_id, visible=False)


def work(thread_id): # thread_id seems useless
  """thread worker"""
  while not task_queue.empty():
    task = task_queue.get()

    run_task(thread_id, task)

    task_queue.task_done()
    overall_progress.update(overall_task_id, advance=1)
    add_n_finished()
    top_descr = "[bold #AAAAAA](%d out of %d tasks finished)" % (n_finished, len(tasks))
    overall_progress.update(overall_task_id, description=top_descr)


# === main thread ===

with Live(progress_group):
  top_descr = "[bold #AAAAAA](%d out of %d tasks finished)" % (n_finished, len(tasks))
  overall_progress.update(overall_task_id, description=top_descr)
  
  threads = []
  for thread_id in range(n_threads):
    thread = threading.Thread(target=work, args=(thread_id,)) 
    thread.start()
    threads.append(thread)
  
  task_queue.join()
  for thread in threads:
    thread.join()

  overall_progress.update(
    overall_task_id, 
    description="[bold green]%s histories checked, done!" % len(tasks)
  )

if checker == 'ours':
  for task in tasks:
    total_time = 0
    timeout = False
    for key, value in results[task].items():
      if value == 'TO':
        timeout = True
        break
      if key.endswith('time') and value.endswith('ms'):
        total_time += int(value[:-2])
    if not timeout:
      results[task]['total time'] = f'{total_time}ms'
    else:
      results[task]['total time'] = 'TO'

with open(output_path, 'w+') as json_file:
  json.dump(results, json_file, indent=2)

table = Table()
table.add_column('History', justify="center")
table.add_column('Time', justify="center")
table.add_column('Memory', justify="center")
table.add_column('Accept', justify="center")

for task in tasks:
  table.add_row(task, 
                results[task]['total time'], 
                'TO' if results[task]['max memory'] == 'TO' else humanize.naturalsize(results[task]['max memory']), 
                "[bold green]✔" if results[task]['accept'] else "[bold red]✘")

console = Console()
console.print(table, justify="center")
