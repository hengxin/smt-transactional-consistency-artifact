import threading
from queue import Queue
import subprocess
from rich.progress import Progress
import time

# 公共队列
tasks_queue = Queue()

# 添加任务到队列
for i in range(10):
    tasks_queue.put(f"Task {i+1}")

# 定义任务执行函数
def run_task(task):
    # 执行子进程命令
    # process = subprocess.run(task.split(), capture_output=True, text=True)
    # 获取子进程的输出
    # output = process.stdout.strip()
    time.sleep(1)
    # 统计数据
    # 这里只是个示例，你可以根据需要进行具体的统计操作
    # data = len(output)
    return 1

# 定义线程函数
def worker():
    while True:
        # 从队列中获取任务
        task = tasks_queue.get()
        if task is None:
            # 队列为空，退出线程
            break
        # 执行任务
        data = run_task(task)
        # 更新进度条
        progress.advance()
        # 打印任务结果
        print(f"Task '{task}' completed. Data: {data}")

# 创建进度条
progress = Progress()
# 添加任务数作为任务量
progress.add_task("[cyan]Processing...", total=tasks_queue.qsize())

# 创建线程列表
threads = []
# 创建并启动四个线程
for _ in range(4):
    thread = threading.Thread(target=worker)
    thread.start()
    threads.append(thread)

# 等待所有线程完成
for thread in threads:
    thread.join()

# 完成进度条
progress.stop()