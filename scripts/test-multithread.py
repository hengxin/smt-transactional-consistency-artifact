# === imports ===
# system
import os
import subprocess
from sys import stderr

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

logging.basicConfig(
  level = logging.DEBUG,  
  format = '[%(asctime)s] [%(levelname)s]  %(message)s',  
  filename = 'test.log',  
  filemode = 'w'  
)

history_type = 'dbcop' 
assert history_type == 'cobra' or history_type == 'dbcop'
logging.info(f'history type = {history_type}')

root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
logging.info(f'root path = {root_path}')
# will run all histories under this path
# history_path = history_path = os.path.join(root_path, 'history', '{}-logs'.format(history_type), 'one-shot-chengRW') 
history_path = os.path.join(root_path, 'history', '{}-logs'.format(history_type), 'uv')
logging.info(f'history path = {history_path}')

checker_path = os.path.join(root_path, 'builddir', 'checker')
logging.info(f'checker path = {checker_path}')

solver = 'monosat'
assert solver == 'acyclic-minisat' or solver == 'monosat' or solver == 'z3'
logging.info(f'solver = {solver}')

pruning = True
logging.info(f'pruning = {pruning}')

n_threads = 4
logging.info(f'use {n_threads} thread(s)')

output_path = os.path.join(root_path, 'results', 'test-results.json')
logging.info(f'output path = {output_path}')

# === global variables ===
tasks = os.listdir(history_path)
task_queue = queue.Queue()
for history_dir in tasks:
  task_queue.put(history_dir)

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

def parse_logs(thread_id, logs, task_name):
  """parse logs of a task and store"""
  logging.debug(f'thread {thread_id} starts parsing logs of task {task_name}, logs = {logs}')
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
  

def run_task(thread_id, task):
  """run a single task"""
  logging.info(f'thread {thread_id} starts running task {task}')

  current_task_id = current_task_progress.add_task("%s" % task, visible=False)
  current_task_steps_id = task_steps_progress.add_task("[bold blue]Thread %d: Running %s" % (thread_id, task))

  history_dir = task
  if history_type == 'cobra':
    bincode_path = str(os.path.join(history_path, history_dir)) + '/'
  else: # dbcop
    bincode_path = os.path.join(history_path, history_dir, 'hist-00000', 'history.bincode')
  
  cmd = [checker_path, bincode_path, '--solver', solver, '--history-type', history_type]
  if pruning:
    cmd.append('--pruning')
  logging.debug(f'thread {thread_id} runs cmd {cmd}')
  result = subprocess.run(cmd, capture_output=True, text=True)
  
  logging.debug(f'thread finished checking {task}, stdout = {result.stdout}, stderr = {stderr}')
  logs = result.stdout.split(os.linesep)
  parse_logs(thread_id, logs, task)
  logging.info(f'thread {thread_id} finished task {task}')

  current_task_progress.stop_task(current_task_id)
  current_task_progress.update(current_task_id, description="[bold green]%s [green]:heavy_check_mark:" % (task, ), visible=True)
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

for task in tasks:
  total_time = 0
  for key, value in results[task].items():
    if key.endswith('time') and value.endswith('ms'):
      total_time += int(value[:-2])
  results[task]['total time'] = f'{total_time}ms'

with open(output_path, 'w+') as json_file:
  json.dump(results, json_file, indent=2)

table = Table()
table.add_column('History', justify="center")
table.add_column('Time', justify="center")
table.add_column('Accept', justify="center")

for task in tasks:
  table.add_row(task, results[task]['total time'], "[bold green]✔" if results[task]['accept'] else "[bold red]✘")

console = Console()
console.print(table, justify="center")