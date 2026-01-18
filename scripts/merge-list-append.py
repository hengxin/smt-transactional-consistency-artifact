import os
import re
import csv

def safe_filename(name: str) -> str:
  # 把 key 转成安全文件名（read-r 这种会保留，其他非法字符替换为 _）
  return re.sub(r'[\\/:*?"<>| ]+', "_", name)

def merge_tools_to_param_csvs(tools_results: dict, output_dir: str = "csv_results_merged", param_values: dict | None = None):
  """
  tools_results: {
    "toolA": {"sess": [...], "txn": [...], ...},
    "toolB": {"sess": [...], "txn": [...], ...},
    ...
  }

  output_dir: 输出目录
  param_values: 可选，用来给每个参数提供真实的参数取值列表
    例如:
      {
        "sess": [1,2,4,8,16,32],
        "txn":  [1,2,4,8,16,32,64,128,256]
      }
    如果不传，则默认用 index 作为参数值
  """
  os.makedirs(output_dir, exist_ok=True)

  # 1) 收集所有参数名（例如 sess/txn/ops/...）
  all_params = set()
  for tool_name, param_dict in tools_results.items():
    all_params.update(param_dict.keys())
  all_params = sorted(all_params)

  # 2) 对每个参数名，合并所有工具的列表
  for param_name in all_params:
    # 找到该参数在所有工具中最大的长度，用于对齐（补空）
    max_len = 0
    for tool_name, param_dict in tools_results.items():
      times = param_dict.get(param_name, [])
      if len(times) > max_len:
        max_len = len(times)

    # 生成参数轴（param/index 列）
    if param_values and param_name in param_values:
      axis = list(param_values[param_name])
      # 如果提供的参数值数量不足/超出，做一个安全对齐
      if len(axis) < max_len:
        axis = axis + list(range(len(axis), max_len))
      elif len(axis) > max_len:
        axis = axis[:max_len]
      axis_col_name = "param"
    else:
      axis = list(range(max_len))
      axis_col_name = "index"

    # 输出文件
    filename = safe_filename(param_name) + ".csv"
    filepath = os.path.join(output_dir, filename)

    tool_names = sorted(tools_results.keys())

    with open(filepath, "w", newline="", encoding="utf-8") as f:
      writer = csv.writer(f)

      # 表头：参数轴 + 各工具列
      header = [axis_col_name] + tool_names
      writer.writerow(header)

      # 每一行：参数值 + 各工具该点时间（无则空）
      for i in range(max_len):
        row = [axis[i]]
        for tool_name in tool_names:
          times = tools_results[tool_name].get(param_name, [])
          row.append(f"{times[i] / 1000:.3f}" if i < len(times) else "")
        writer.writerow(row)

  print(f"✅ 已按参数合并生成 {len(all_params)} 个 CSV，输出目录：{output_dir}")


if __name__ == "__main__":
  # 示例：两个工具的结果（你按实际替换/追加更多工具）
  ours_rw = {
    'sess': [89.23438129325707, 356.954044662416, 903.6618573591113, 1875.374210998416, 3165.5544782988727, 4998.012570043405], 
    'txn': [27.551090500007074, 64.87601296976209, 124.42345436041553, 223.06971717625856, 361.74679500982165, 1867.3757828461628, 4925.920821415882, 10125.671065567682, 16199.306784197688], 
    'ops': [167.57370935132107, 447.25099205970764, 1007.020402389268, 1852.9168719736238, 3074.4527972613773, 4915.085211706658], 
    'keys': [4750.325819011778, 2342.3958170848587, 1535.8638504209619, 1198.4320073388517, 974.1421535921594], 
    'read-r': [40966.18277346715, 4417.490490557005, 1880.4647858875494, 774.6528318772713, 218.15698593854904], 
    'dup-r': [1881.6732461564243, 1806.7491641268134, 1980.0269485761721, 1894.4956477110584, 1931.6191736919184, 1859.326366490374]
  }


  ours_list = {
    'sess': [8433.5746432965, 17564.734787214547, 23935.75989641249, 27104.810736762982, 31483.926103450358, 34003.69621021673], 
    'txn': [2112.9283350892365, 6679.337102298935, 10729.982776256898, 14770.296174567193, 17831.60730296125, 27078.463495398562, 34221.28465414668, 37826.27112201104, 43121.588627963014], 
    'ops': [8707.23044484233, 17943.135708725702, 22525.15575538079, 27031.814803679783, 31247.81972542405, 31789.5586383529], 
    'keys': [8788.269465633979, 20187.238655673962, 34692.29516976824, 54718.534629791975, 71468.31823994096], 
    'read-r': [7057.55051675563, 21210.2584472547, 27050.107469471794, 29470.076148553442, 29537.326948406797], 
    'dup-r': [27762.209940701723, 27793.93341563021, 28006.298351412017, 26097.808580379933, 28248.69941469903, 27529.794081269454]
  }


  tools_results = {
    "ours-rw": ours_rw,
    "ours-list": ours_list
  }

  # 可选：给某些参数提供真实参数值（不传就用 index）
  params = {
    "sess": [5, 10, 15, 20, 25, 30],
    "txn":  [10, 20, 30, 40, 50, 100, 150, 200, 250],
    "ops":  [5, 10, 15, 20, 25, 30],
    "keys":  [2000, 4000, 6000, 8000, 10000],
    "read-r":  [5, 25, 50, 75, 95],
    "dup-r":  [0, 20, 40, 60, 80, 100],
  }

  merge_tools_to_param_csvs(tools_results, output_dir="csv_results_merged", param_values=params)
