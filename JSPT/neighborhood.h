#pragma once

#include <vector>
#include "Chromosome.h"
#include "critical_path.h"
#include "GONGJIAN.h"
#include "yunshutime.h"

// ============================================================
// N8 邻域 — 基于关键路径机器块的工序交换邻域
//
// 参考: Nowicki & Smutnicki (1996)
// N8 邻域对每个机器关键块，根据其位置（首/内部/尾）交换
// operation_sequence 中的对应工序，生成邻居解。
//
// 块位置规则:
//   FIRST    — 仅交换块内最后两个工序（避免扰动源点附近的紧前驱关系）
//   LAST     — 仅交换块内前两个工序（避免扰动 makespan 终点的后驱关系）
//   INTERNAL — 交换前两个工序 和 最后两个工序（两对）
//   若块长度 == 2，两对重合，由去重逻辑自动合并。
//
// API 分层设计:
//   generateN8Moves()  → 收集所有合法 swap 的描述符（不计算）
//   applyN8Move()      → 对单个 swap 拷贝染色体、执行交换、重新解码
//   generateN8Neighbors() → 便捷组合，一步得到所有邻居（调试用）
//
// 禁忌搜索典型调用流程:
//   1. moves = generateN8Moves(current, cp, jobs)
//   2. for each move: 评估 fitness，查 tabu list，选最优
//   3. current = applyN8Move(current, best_move, yt, jobs)
// ============================================================

// N8 邻域操作：在 operation_sequence 中交换两个位置的工序
struct N8Move {
    int pos1;           // OS 中第一个交换位置 (0-based)
    int pos2;           // OS 中第二个交换位置 (0-based)
    int block_resource; // 来源关键块的机器 id（调试/禁忌记录用）
};

// 生成当前解的所有 N8 邻域操作描述符。
// 仅扫描关键块并计算 OS 位置，不执行 calculate()。
// 自动对重复 swap 去重（两个不同关键块可能产生同一对 swap）。
// 前置条件: chromosome.calculate() 已执行，cp.valid == true。
std::vector<N8Move> generateN8Moves(
    const Chromosome& chromosome,
    const CriticalPathResult& cp,
    const std::vector<GONGJIAN>& jobs);

// 应用单个 N8 操作，拷贝染色体、交换 OS 中两个位置、重新解码。
// 返回的邻居染色体已调用 calculate()，可直接读取 fitness。
Chromosome applyN8Move(
    const Chromosome& chromosome,
    const N8Move& move,
    const yunshutime& yt,
    const std::vector<GONGJIAN>& jobs);

// 便捷函数：生成所有 N8 邻居染色体（包含完整解码结果）。
// 等价于 generateN8Moves + 对每个 move 调用 applyN8Move。
// 用于测试、调试和小规模评估。
std::vector<Chromosome> generateN8Neighbors(
    const Chromosome& chromosome,
    const CriticalPathResult& cp,
    const std::vector<GONGJIAN>& jobs,
    const yunshutime& yt);

// ============================================================
//  N8 邻域验证 — 逐块逐 move 详查，证明 N8 规则正确性
//
//  打印每个机器关键块的完整内容、位置分类、N8 选中的交换对，
//  并对每条 move 进行规则符合性检查:
//    (a) 交换的两个工序是否真的在同一个机器块内且连续？
//    (b) 块位置与交换对选择是否符合 N8 规则？
//    (c) 交换后 OS 中各工件出现次数是否不变？
//    (d) 去重是否有效（无重复 move）？
//
//  参数:
//    chromosome : 已 calculate() 的解
//    cp         : findCriticalPath() 的结果
//    jobs       : 问题实例
//    yt         : 运输时间
//  返回:
//    0 = 全部检查通过, 非0 = 发现问题数
// ============================================================
int verifyN8Detailed(
    const Chromosome& chromosome,
    const CriticalPathResult& cp,
    const std::vector<GONGJIAN>& jobs,
    const yunshutime& yt);
