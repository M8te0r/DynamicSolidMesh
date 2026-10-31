# Slot map 对比基准

基准只测试项目当前 `m_slot_map.h` 与 `slot_map.h` 中 `dod::slot_map64` 的元素存储，不修改两个容器，
也不执行合法网格构造、拓扑维护或事务回滚。构建目标默认关闭，不影响日常单元测试。

测试用 `mesh_element.h` 中的 Vertex、Dedge、Halfface、Cell。
Vertex 的 outgoing 长度为 6–8，Dedge 的 incident_halffaces 长度为 4–6；
其中存放默认占位 handle，不代表一张合法网格。Halfface/Cell 使用其实际定长布局。
插入计时包含构造这些元素及其邻接 vector 的成本，因此并非纯容器指令成本。

## 复现

在项目根目录执行（Clang/GCC，C++17；基准使用编译器屏障防止优化掉读取）：

```sh
cmake -S . -B build/slotmap-bench -DCMAKE_BUILD_TYPE=Release -DBUILD_SLOT_MAP_BENCHMARKS=ON
cmake --build build/slotmap-bench --parallel 2
ctest --test-dir build/slotmap-bench --output-on-failure
build/slotmap-bench/slot_map_bench smoke
mkdir -p benchmarks/results/local
build/slotmap-bench/slot_map_bench metadata > benchmarks/results/local/metadata.csv
build/slotmap-bench/slot_map_bench timing 7 10000 100000 1000000 > benchmarks/results/local/timing.csv
build/slotmap-bench/slot_map_memory memory 10000 100000 1000000 > benchmarks/results/local/memory.csv
python3 benchmarks/summarize.py benchmarks/results/local
```

计时和内存进程应顺序运行，不要同时执行。运行时尽量避免其他重负载。
`metadata.csv` 给出真实类型大小、对齐、移动性质和键大小。

## 计时方法

- 单线程，Release `-O3 -DNDEBUG`，不使用 sanitizer 或分配统计钩子计时。
- 每种类型和规模先预热一轮，再计时七轮；每轮交替两个容器的先后次序。
- 同轮输入相同；随机种子固定。查询排列与删除排列独立，避免把失效 key 集中到前半段。
- key、随机索引、预期值等测试驱动数据在计时外分配；key 写入本身包含在插入和删插计时中。
- 查找和遍历每轮约处理两百万次；同一轮中重复使用该轮的随机排列，每次重复前有编译器内存屏障。
  这是重复访问工作集的测试，会受缓存和分支预测学习影响。输出校验和及数量不匹配会终止测试。
- 基于已分配、已触碰的数据测量；不是冷缓存测试。不锁 CPU 核心、频率或分配器缓存状态。
- 默认 dod 参数保持 4096 槽/页、空闲队列超过 64 才复用；自定义保持 vector 自动增长和空槽即时复用。
  当前自定义版本使用独立的下标数组作为空闲栈；预留容量覆盖全部槽位，删除不扩容。
  早期结果使用槽内链表；另有计数缓冲区的评估实验，详见各次报告。
  基准的普通 append 没有调用新增的 reserve()，保留自动扩容成本。

每轮执行：从空容器插入 → 有效 key 查询 → 密集遍历 → 密集复制/析构副本 →
随机删除半数 → 混合有效/失效查询 → 半稀疏遍历 → 回填半数 → 随机删插对 →
删除到一成存活 → 稀疏遍历 → 析构。每种类型还单独测量已有 65536 个元素时的下一次插入。
该边界在本次 libc++ 的 vector 增长策略与 dod 的页大小下均触发存储增长；它是特定边界事件，
不能代表普通插入的平均耗时或总体 p99。

`iterate_keys_*` 对两者都遍历 key 再调用 get；dod 使用只读访问的非 const `items()`，
避开其 const 迭代器通过 const_cast 修改 const 子对象的路径。
`iterate_native_*` 允许 dod 直接遍历对象，自定义仍使用 keys()+get。这两组分别展示
相同 handle 调用方式和各自现有原生接口的实际成本。

`get_random_live` 直接调用各自的 get；另列 contains 再 get 的结果，因为两个实现的边界校验并不完全相同。
随机删插测试维持元素数量不变，一项包括删除及新元素构造、插入，共两个操作。

## 已知正确性限制

只对**没有删除槽位**的源容器计时复制构造。当前 dod 已加入跳过删除槽位的复制检查，
但仍未复制空闲队列；因此含空洞复制双方均跳过，不把不完整实现的耗时当作性能数据。
自定义版本的稀疏复制正确性由项目单元测试覆盖。没有计时自赋值、失败回滚或复制赋值。

构造、复制、查找、删除和遍历均校验结果；基准本身还以小规模 smoke 在 ASan/UBSan 下运行。
这些检查不能代替完整容器正确性证明，性能结果也不代表 dod 的已知复制问题已修复。

## 内存口径

`slot_map_memory` 是单独编译的进程：替换全局 new/delete，并利用 dod 已提供的
SLOT_MAP_ALLOC/FREE 接口记录分配。计时进程没有这些钩子。

计入容器数组、容量余量、页面、空闲队列及元素成员 vector 的请求字节数；
排除测试驱动的 keys/随机数组、容器栈对象、记录头、分配器对齐取整和运行库内部缓存。
这里的“峰值”是该阶段同时存活的请求字节数最大值，捕获 vector 重分配期间新旧数组并存。
它不是 RSS，也不是操作系统实际承诺/驻留内存。分配调用次数包含元素成员的分配。

内存阶段依次为：空容器 → 密集填充 → 同时保留源和密集副本 → 删除副本 →
删除一半 → 仅剩一成 → 回填至原数量 → 全部删除。最后验证被统计的堆分配全部释放。

## 结果文件

最近一次调整及取舍见 [2026-10-31 综合调整报告](results/2026-10-31-balanced-review/REPORT.md)。

- `timing.csv`：每轮原始总时间、工作量及 ns/项。
- `statistics.csv`：每个场景的中位数、Q1/Q3、最小/最大值及样本数。
- `memory.csv`：各阶段当前请求字节数、峰值及分配次数。
- `metadata.csv`：类型布局和键大小。
- `SUMMARY.md`：完整表格。小幅差异应结合波动区间解释，不据此宣称普遍优胜。
