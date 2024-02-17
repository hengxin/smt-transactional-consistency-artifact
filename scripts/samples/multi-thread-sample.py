import subprocess
import threading
import queue
from rich.progress import Progress, SpinnerColumn, TimeElapsedColumn, TextColumn, BarColumn, TaskProgressColumn
import time

# 定义任务队列
task_queue = queue.Queue()

# 向任务队列中添加任务
tasks = ["task1", "task2", "task3", "task4", "task5"]
for task in tasks:
    task_queue.put(task)

# 定义线程数
num_threads = 4

# 定义执行任务的函数
def execute_task(progress, task_bar):
    while not task_queue.empty():
        # 从队列中获取任务
        task = task_queue.get()
        
        # 执行子进程命令

        time.sleep(1)
        # process = subprocess.run(task, capture_output=True, text=True, shell=True)
        
        # 统计子进程输出
        # output = process.stdout
        # 在这里添加你的统计逻辑
        # 例如，你可以对输出进行处理、分析或打印
        
        # 更新进度条
        progress.update(task_bar, advance=1)
        
        # 标记任务完成
        task_queue.task_done()

# 创建进度条
with Progress(SpinnerColumn(),
            TextColumn("[progress.description]{task.description}"),
            BarColumn(),
            TaskProgressColumn(),
            TimeElapsedColumn(),) as progress:

    task_bar = progress.add_task("[cyan]任务进度", total=len(tasks))
    
    # 创建并启动线程
    threads = []
    for _ in range(num_threads):
        thread = threading.Thread(target=execute_task, args=(progress, task_bar,))
        thread.start()
        threads.append(thread)
    
    # 等待所有任务完成
    task_queue.join()
    
    # 等待所有线程退出
    for thread in threads:
        thread.join()

print("所有任务已完成")