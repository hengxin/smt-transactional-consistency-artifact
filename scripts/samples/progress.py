import time
import logging

from rich.progress import Progress

logging.basicConfig(
  level = logging.INFO,  
  format = '[%(asctime)s] [%(levelname)s]  [%(message)s]',  
  filename = 'test.log',  
  filemode = 'w'  
)

with Progress() as progress:

    task1 = progress.add_task("[red]Downloading...", total=1000)
    task2 = progress.add_task("[green]Processing...", total=1000)
    task3 = progress.add_task("[cyan]Cooking...", total=1000)

    while not progress.finished:
        progress.update(task1, advance=0.5)
        progress.update(task2, advance=0.3)
        progress.update(task3, advance=0.9)
        print('111')
        time.sleep(0.02)