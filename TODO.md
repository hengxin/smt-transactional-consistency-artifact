- [x] 把加进去的边存下来，删除的时候直接倒过来删除，就不用再跑一遍
- [x] 将 known graph 可以推断出的 RW 边和 变量 + 变量 推断出的 RW 边分离，进一步实现 unit edge propagtion 和 搜索顺序的替换
  - [ ] 实现 unit edge propagation
  - [ ] 实现先搜 max edge
- [x] 将 helper 中 ww 和 wr 边的存储方式换成 `from -> (key -> to)` 的结构
- [ ] 增强 no unique value 下 solver 的效果
  - [x] 造一些强大一点的 benchmark，最好写一个自动生成数据的脚本
  - [ ] 准备一些 hard test
  - [ ] 在 theory solver 中实现 $x_1 + x_2 + \ldots + x_n \leq 1$ 的效果，当加入 wr var 时，把这个功能实现在 unit edge propagate 中

- [x] 完善测试脚本
  - [x] 将测试脚本改成多线程的
  - [x] 在测试脚本中加入检测内存的功能
  - [x] 准备一个脚本把 `.json` 格式的数据整理成 `.csv`
  - [x] 完成 polysi-fig7-like 的画图 latex 代码
                                  