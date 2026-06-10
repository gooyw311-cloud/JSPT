# JSPT — Job Shop Scheduling with Transport

> 带 AGV 运输约束的作业车间调度问题，基于禁忌搜索 (Tabu Search) 与遗传算法 (Genetic Algorithm) 的元启发式求解框架。

## 问题描述

经典 Job Shop Scheduling (JSP) 的扩展：在 n 工件 × m 机器的加工调度基础上，引入 v 辆 AGV 的运输约束，形成**双资源约束**（加工资源 + 运输资源）的优化问题。

- **目标**：最小化 makespan（最大完工时间）
- **运输模型**：空载移动 + 满载移动，AGV 从当前位置前往 pickup 点再运送工件
- **算例兼容**：同时支持 BU/SWV 和 HK 两种算例格式

## 技术栈

- **语言**：C++17
- **构建**：MSBuild (Visual Studio 2022, v143 toolchain)，x64
- **编码**：UTF-8 with BOM，CRLF

## 算法实现

### 染色体编码

双层编码结构：

| 层 | 内容 | 说明 |
|----|------|------|
| Layer 1 | 车辆分配 `vehicle_assignment[j][d]` | 为每段运输指定 AGV 编号 |
| Layer 2 | 加工顺序 `operation_sequence` | 所有工件按出现顺序 list-scheduling 解码 |

### 解码器

基于贪心 list-scheduling 的解码器，维护 `vehicle_free[]` / `vehicle_pos[]` / `machine_free[]` 三条时间轴，按 OS 顺序逐个安排运输→加工。

### 关键路径分析

逆向追溯识别关键路径，定位**机器关键块**和**车辆关键块**，按 N8 邻域结构分类（FIRST / INTERNAL / LAST），为邻域搜索提供优化方向。

### 禁忌搜索 (进行中)

- **邻域结构**：N8 交换、k-insertion、车辆重分配/交换
- **自适应机制**：k 值权重动态调整
- **禁忌表**：分离不同邻域类型的禁忌长度
- **藐视准则**：优于历史最优解时破禁

### 遗传算法 (规划中)

- 选择 / 交叉 (POX, JOX, PBX) / 变异算子
- 精英保留策略
- TS vs GA 对比实验

## 项目结构

```
JSPT/
├── GONGJIAN.h / GONGXU.h       # 工件 & 工序数据结构
├── yunshutime.h                # 运输时间矩阵 (man / kong / man_hk)
├── Chromosome.h / .cpp         # 染色体定义 + 调度解码器
├── random.h / .cpp             # 随机初始解生成
├── read_instances.h / .cpp     # 算例读取 (BU/SWV + HK)
├── critical_path.h / .cpp      # 关键路径分析
├── neighborhood.h / .cpp       # N8 + k-insertion + vehicle 邻域
├── tabu_search.h / .cpp        # 禁忌搜索主算法 (待完成)
├── 源.cpp                      # 主入口
├── EX101.txt / SWV1.txt        # BU/SWV 测试算例
├── HK1.txt                     # HK 测试算例
└── JSPT.sln                    # VS2022 解决方案
```

## 构建与运行

```bash
# 编译
MSBuild JSPT.sln /p:Configuration=Release /p:Platform=x64

# 运行
./x64/Release/JSPT.exe

# 甘特图输出 (HTML)
#   BU/SWV → gantt_chart.txt / gantt_chart_swv.txt
#   HK     → gantt_chart_hk.txt
```

## 参考文献

- Nowicki, E., & Smutnicki, C. (1996). A fast taboo search algorithm for the job shop problem. *Management Science*, 42(6), 797-813.
- Zhang, Q., et al. (2020). Flexible job-shop scheduling with AGV transportation constraints.

---

*中南财经政法大学 信息工程学院 课程/论文项目*
