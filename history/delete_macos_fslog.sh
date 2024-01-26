#!/bin/bash

# 指定文件夹路径
folder_path="."

# 使用 find 命令递归查找以 . 开头的文件，并删除它们
find "$folder_path" -type f -regex '.*/\..*' -exec rm {} +

echo "已递归删除以 . 开头的文件"