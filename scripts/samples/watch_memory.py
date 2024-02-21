import subprocess
import psutil

def monitor_memory(process):
    process_id = process.pid
    max_memory = 0

    while process.poll() is None:
        try:
            process_info = psutil.Process(process_id)
            memory_info = process_info.memory_info()
            memory_usage = memory_info.rss
            max_memory = max(max_memory, memory_usage)

        except psutil.NoSuchProcess:
            break

    return max_memory

# 启动子进程
process = subprocess.Popen(['your_command'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

# 监视内存使用量
max_memory_used = monitor_memory(process)
print(f"Maximum memory used: {max_memory_used} bytes")