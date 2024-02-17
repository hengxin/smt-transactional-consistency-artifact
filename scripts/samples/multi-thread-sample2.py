import threading
import time
from loguru import logger
from tqdm import tqdm

# 自定义日志记录器
class CustomLogger:
    def __init__(self):
        self.current_task = {}
        self.lock = threading.Lock()

    def set_task(self, thread_name, task):
        with self.lock:
            self.current_task[thread_name] = task

    def remove_task(self, thread_name):
        with self.lock:
            self.current_task.pop(thread_name, None)

    def print_tasks(self):
        with self.lock:
            tasks = [f'{thread_name}: {task}' for thread_name, task in self.current_task.items()]
            return ', '.join(tasks)

# 创建自定义日志记录器实例
custom_logger = CustomLogger()

# 配置 loguru 记录器
logger.add("app.log", rotation="500 MB")  # 将日志写入文件，按大小进行切割

# 定义线程执行的任务
def run_task(task):
    thread_name = threading.current_thread().getName()
    custom_logger.set_task(thread_name, task)
    logger.info(f'线程 {thread_name} 正在执行任务: {task}')
    time.sleep(2)
    logger.info(f'线程 {thread_name} 完成任务: {task}')
    custom_logger.remove_task(thread_name)

# 创建并启动线程
threads = []
for i in range(5):
    thread = threading.Thread(target=run_task, args=(f'任务{i+1}',))
    thread.start()
    threads.append(thread)

# 创建进度条
pbar = tqdm(total=len(threads), ncols=80)

# 实时更新任务信息
while threads:
    pbar.set_description(custom_logger.print_tasks())
    pbar.refresh()
    time.sleep(0.1)

# 关闭进度条
pbar.close()