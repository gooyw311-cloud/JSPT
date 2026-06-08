#include "critical_path.h"
#include <set>
#include <tuple>
#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <cstdio>

using namespace std;

namespace {

// ---- 节点标识打包 (用于 visited 集合) ---------------------------------------
// 格式: (type_int, job, index)   type_int: 0=OPERATION, 1=TRANSPORT
using NodeKey = tuple<int, int, int>;

NodeKey makeKey(CPNodeType type, int job, int index) {
    return { static_cast<int>(type), job, index };
}
NodeKey makeKey(const CPNode& node) {
    return { static_cast<int>(node.type), node.job, node.index };
}

// ---- 构造 CPNode 的函数 -------------------------------------------------

CPNode makeOperationNode(int j, int d,
                          const vector<GONGJIAN>& jobs,
                          const Chromosome& chro) {
    CPNode node;
    node.type     = CPNodeType::OPERATION;
    node.job      = j;
    node.index    = d;
    node.resource = jobs[j].gongxu_set[d].jiqi_id;
    node.start    = chro.op_start[j][d];
    node.end      = chro.op_end[j][d];
    return node;
}

CPNode makeEmptyTransportNode(int j, int d, const Chromosome& chro) {
    CPNode node;
    node.type     = CPNodeType::TRANSPORT_EMPTY;
    node.job      = j;
    node.index    = d;
    node.resource = chro.vehicle_assignment[j][d];
    node.start    = chro.empty_start[j][d];  // 空载开始 = 车辆空闲时刻
    node.end      = chro.empty_end[j][d];    // 空载结束 = 车辆到达 pickup 点
    return node;
}

CPNode makeLoadedTransportNode(int j, int d, const Chromosome& chro) {
    CPNode node;
    node.type     = CPNodeType::TRANSPORT_LOADED;
    node.job      = j;
    node.index    = d;
    node.resource = chro.vehicle_assignment[j][d];
    node.start    = chro.trans_start[j][d];  // 满载开始 = max(空载到达, 工件就绪)
    node.end      = chro.trans_end[j][d];    // 满载结束
    return node;
}

// ---- 找工序的机器前驱 -------------------------------------------------------
// 遍历所有工序，找出与 O_{j,d} 同机器且结束时间 == op_start[j][d] 的工序。
// 这些工序可能就是执行完后立即释放机器给 O_{j,d} 的前驱。
vector<CPNode> findMachinePredecessors(
    int j, int d,
    const Chromosome& chro,
    const vector<GONGJIAN>& jobs)
{
    vector<CPNode> preds;
    int machine_id  = jobs[j].gongxu_set[d].jiqi_id;
    int start_time  = chro.op_start[j][d];

    if (start_time == 0) return preds;          // 源点，没有前驱

    int num_jobs = static_cast<int>(jobs.size());
    for (int jj = 0; jj < num_jobs; ++jj) {
        int n_ops = static_cast<int>(chro.op_end[jj].size());
        for (int dd = 0; dd < n_ops; ++dd) {
            if (jj == j && dd == d) continue;   // 跳过自己
            if (jobs[jj].gongxu_set[dd].jiqi_id == machine_id
                && chro.op_end[jj][dd] == start_time) {
                preds.push_back(makeOperationNode(jj, dd, jobs, chro));
            }
        }
    }
    return preds;
}

// ---- 找空载运输的车辆前驱 ---------------------------------------------------
// 空载运输 E_{j,d} 的开始时刻 empty_start = vehicle_free[v_id]，
// 而 vehicle_free 被上一趟满载运输的 trans_end 更新。
// 因此前驱是：同车辆上 trans_end == empty_start[j][d] 的满载运输。
vector<CPNode> findVehiclePredecessors(
    int j, int d,
    const Chromosome& chro,
    const vector<GONGJIAN>& jobs)
{
    vector<CPNode> preds;
    int v_id        = chro.vehicle_assignment[j][d];
    int empty_st    = chro.empty_start[j][d];

    if (v_id < 1  || empty_st == 0) return preds;  // 无效车辆 或 车辆初始空闲

    int num_jobs = static_cast<int>(jobs.size());
    for (int jj = 0; jj < num_jobs; ++jj) {
        int n_trans = static_cast<int>(chro.vehicle_assignment[jj].size());
        for (int dd = 0; dd < n_trans; ++dd) {
            if (jj == j && dd == d) continue;
            if (chro.vehicle_assignment[jj][dd] == v_id
                && chro.trans_end[jj][dd] == empty_st) {
                // 前驱是满载运输——它结束的时刻释放了车辆
                preds.push_back(makeLoadedTransportNode(jj, dd, chro));
            }
        }
    }
    return preds;
}

// ---- 找节点的所有关键前驱 ---------------------------------------------------
// 关键前驱 = 决定了当前节点开始时间的那个（些）节点。
// 若多个前驱的 end time == 当前节点的 start time（max 并列），全部收集。
//
// === 工序 O_{j,d} ===
//   候选前驱（对应 op_start = max 的三项）:
//     ① 满载运输 L_{j,d}      — trans_end[j][d] == op_start[j][d]
//     ② 同工件上一工序 O_{j,d-1} — d>0 && op_end[j][d-1] == op_start[j][d]
//     ③ 同机器上一工序        — machine_free[m_id] == op_start[j][d]
//
// === 空载运输 E_{j,d} ===
//   候选前驱（empty_start = vehicle_free[v_id]）:
//     ① 同车辆上一个满载运输   — trans_end[j'][d'] == empty_start[j][d]
//
// === 满载运输 L_{j,d} ===
//   候选前驱（trans_start = max(empty_end, job_ready)）:
//     ① 同段空载运输 E_{j,d}  — empty_end[j][d] == trans_start[j][d]
//     ② 同工件上一工序 O_{j,d-1} — d>0 && op_end[j][d-1] == trans_start[j][d]
// ============================================================
vector<CPNode> findCriticalPredecessors(
    const CPNode& node,
    const Chromosome& chro,
    const vector<GONGJIAN>& jobs,
    const yunshutime& yt)
{
    vector<CPNode> preds;
    int j = node.job;
    int d = node.index;
    bool is_hk = (yt.suanlileibei == "HK");

    if (node.type == CPNodeType::OPERATION) {
        // ============================================================
        //  工序 O_{j,d} 的前驱
        // ============================================================
        int op_st = chro.op_start[j][d];
        if (op_st == 0) return preds;                     // 源点

        // ① 满载运输前驱: L_{j,d} 把工件送到机器上
        //    HK 第一道工序没有运输依赖 (job 已在机器上)
        if (!(is_hk && d == 0)) {
            if (d < static_cast<int>(chro.trans_end[j].size())
                && chro.trans_end[j][d] == op_st) {
                preds.push_back(makeLoadedTransportNode(j, d, chro));
            }
        }

        // ② 同工件前驱 (job predecessor)
        if (d > 0 && chro.op_end[j][d - 1] == op_st) {
            preds.push_back(makeOperationNode(j, d - 1, jobs, chro));
        }

        // ③ 同机器前驱 (machine predecessor)
        {
            auto mp = findMachinePredecessors(j, d, chro, jobs);
            preds.insert(preds.end(), mp.begin(), mp.end());
        }

    } else if (node.type == CPNodeType::TRANSPORT_EMPTY) {
        // ============================================================
        //  空载运输 E_{j,d} 的前驱
        // ============================================================
        int empty_st = chro.empty_start[j][d];
        if (empty_st == 0) return preds;                   // 车辆初始空闲，源点

        // ① 车辆前驱: 同车辆的上一趟满载运输结束 == 空载开始
        {
            auto vp = findVehiclePredecessors(j, d, chro, jobs);
            preds.insert(preds.end(), vp.begin(), vp.end());
        }

    } else {
        // ============================================================
        //  满载运输 L_{j,d} 的前驱 (CPNodeType::TRANSPORT_LOADED)
        // ============================================================
        int trans_st = chro.trans_start[j][d];
        if (trans_st == 0) return preds;                   // 源点

        // ① 空载运输前驱: 同段空载到达时刻 == 满载开始时刻
        if (d < static_cast<int>(chro.empty_end[j].size())
            && chro.empty_end[j][d] == trans_st) {
            preds.push_back(makeEmptyTransportNode(j, d, chro));
        }

        // ② 工件前驱: 上一道工序完成时刻 == 满载开始时刻 (工件就绪)
        if (d > 0 && chro.op_end[j][d - 1] == trans_st) {
            preds.push_back(makeOperationNode(j, d - 1, jobs, chro));
        }
    }

    return preds;
}

// ---- 查询节点的开始时间 (用于拓扑排序中的优先级) ----------------------------
int nodeStartTime(const CPNode& node) {
    return node.start;
}

// ---- 将关键路径上的连续同资源节点聚合成块 -----------------------------------
// 沿 path[] 扫描：遇到连续的、同资源（车辆/机器）的节点 → 归入一个块。
// 工序节点按机器聚合；空载和满载运输节点按车辆聚合（可混合）。
// 块长度 ≥ 2 才算有效关键块（长度为 1 的孤点无法交换）。
//
// 块位置判定规则 (N8):
//   FIRST    — 块起始于 path[0]（包含关键路径源点）
//   LAST     — 块结束于 path 末尾（包含关键路径终点 makespan）
//   INTERNAL — 其余
//   若块既首又尾（整条路径就是一个块），按 FIRST 处理。
void extractBlocks(CriticalPathResult& result) {
    int n = static_cast<int>(result.path.size());
    if (n < 2) return;

    int i = 0;
    while (i < n) {
        const CPNode& head = result.path[i];
        bool head_is_op = (head.type == CPNodeType::OPERATION);

        // 聚合：工序按 (类型 + 资源) 聚合；空载/满载运输按资源聚合（允许混合）
        int j = i;
        while (j + 1 < n) {
            const CPNode& next = result.path[j + 1];
            bool next_is_op = (next.type == CPNodeType::OPERATION);

            bool same_group;
            if (head_is_op) {
                // 工序块：必须同是工序、同机器
                same_group = next_is_op && next.resource == head.resource;
            } else {
                // 车辆块：空载/满载可混合，按车辆 id 聚合
                same_group = !next_is_op && next.resource == head.resource;
            }

            if (same_group) {
                ++j;
            } else {
                break;
            }
        }

        int block_len = j - i + 1;
        if (block_len >= 2) {
            CriticalBlock block;
            block.resource_id      = head.resource;
            block.is_machine_block = head_is_op;  // 工序块 vs 车辆块
            block.path_indices.reserve(block_len);
            for (int k = i; k <= j; ++k) {
                block.path_indices.push_back(k);
            }

            // 位置分类
            if (i == 0 && j == n - 1) {
                // 既首又尾：整条路径就这一个块。按 FIRST 处理（不动源点优先）。
                block.position = BlockPosition::FIRST;
            } else if (i == 0) {
                block.position = BlockPosition::FIRST;
            } else if (j == n - 1) {
                block.position = BlockPosition::LAST;
            } else {
                block.position = BlockPosition::INTERNAL;
            }

            if (block.is_machine_block) {
                result.machine_blocks.push_back(block);
            } else {
                result.vehicle_blocks.push_back(block);
            }
        }

        i = j + 1;   // 跳过已处理的块
    }
}

} 


CriticalPathResult findCriticalPath(
    const Chromosome& chromosome,
    const vector<GONGJIAN>& jobs,
    const yunshutime& yt)
{
    CriticalPathResult result;

    // ---- 前置条件：calculate() 必须已执行 ----
    if (chromosome.op_end.empty() || jobs.empty()) {
        result.valid = false;
        return result;
    }

    bool is_hk   = (yt.suanlileibei == "HK");
    int num_jobs = static_cast<int>(jobs.size());
    int makespan = chromosome.fitness;

    // ============================================================
    //  Step 1 — 找终点 (sink nodes)
    //    HK  : makespan = max_j op_end[j][n_ops-1]  → OPERATION 节点
    //    BU/SWV: makespan = max_j trans_end[j][n_ops] → TRANSPORT_LOADED 节点
    // ============================================================
    vector<CPNode> sinks;
    for (int j = 0; j < num_jobs; ++j) {
        if (is_hk) {
            int n_ops = static_cast<int>(chromosome.op_end[j].size());
            if (n_ops > 0 && chromosome.op_end[j][n_ops - 1] == makespan) {
                sinks.push_back(makeOperationNode(j, n_ops - 1, jobs, chromosome));
            }
        } else {
            int n_trans = static_cast<int>(chromosome.trans_end[j].size());
            // BU/SWV: 最后一段运输是返回 LU (d == n_ops)，类型为满载运输
            if (n_trans > 0 && chromosome.trans_end[j][n_trans - 1] == makespan) {
                sinks.push_back(makeLoadedTransportNode(j, n_trans - 1, chromosome));
            }
        }
    }

    if (sinks.empty()) {
        // 没有找到 makespan 对应的节点（异常情况）
        result.valid = false;
        return result;
    }

    // ============================================================
    //  Step 2 — 逆向追溯，构建关键路径
    //   从 sink 出发，沿着关键前驱链一直回到源点。
    //   遇到并列前驱（max 等值）时取第一个，保证得到单条路径。
    // ============================================================
    vector<CPNode> rev_path;            // sink → source 顺序
    set<NodeKey>  visited;              // 防环

    CPNode current = sinks[0];               // 取第一个 sink（若有多个同值任选）
    while (true) {
        NodeKey key = makeKey(current);
        if (visited.count(key)) break;       // 已访问过，停止（理论上不会发生）
        visited.insert(key);

        rev_path.push_back(current);

        // 找当前节点的关键前驱
        auto preds = findCriticalPredecessors(current, chromosome, jobs, yt);

        if (preds.empty()) break;            // 到达源点（无前驱）

        // 多个并列前驱时按 (type, job, index) 字典序取最小，保证确定性。
        // 实际上应取 start time 最大者（最可能决定当前节点开始时间），
        // 但并列时已都相等，因此字典序已足够。
        current = preds[0];
        for (size_t p = 1; p < preds.size(); ++p) {
            if (preds[p] < current) current = preds[p];
        }
    }

    // 翻转得到 source → sink 的正序路径
    reverse(rev_path.begin(), rev_path.end());
    result.path = move(rev_path);

    // ============================================================
    //  Step 3 — 从路径中提取关键块
    // ============================================================
    extractBlocks(result);

    result.valid = true;
    return result;
}

// ============================================================
//  printCriticalPathTrace
//
//  打印关键路径的文本追溯链。
//  按 source→sink 顺序列出每个节点，标注前驱与本节点的 end/start
//  等值关系，最后做自洽性验证——对每对相邻节点检查 end==start。
//  这是证明关键路径正确的核心输出。
// ============================================================
void printCriticalPathTrace(const CriticalPathResult& result)
{
    if (!result.valid) {
        printf("关键路径无效，无法打印追溯链。\n");
        return;
    }

    int n = static_cast<int>(result.path.size());

    printf("\n========== 关键路径追溯验证 ==========\n");
    printf("路径节点数: %d\n", n);
    printf("机器关键块: %d 个\n", static_cast<int>(result.machine_blocks.size()));
    printf("车辆关键块: %d 个\n", static_cast<int>(result.vehicle_blocks.size()));

    // ---- 辅助：节点紧凑表示 ----
    auto nodeLabel = [](const CPNode& node) -> string {
        char buf[32];
        const char* type_str;
        switch (node.type) {
            case CPNodeType::OPERATION:        type_str = "O";  break;
            case CPNodeType::TRANSPORT_EMPTY:  type_str = "E";  break;
            case CPNodeType::TRANSPORT_LOADED: type_str = "L";  break;
            default:                           type_str = "?";  break;
        }
        snprintf(buf, sizeof(buf), "%s[%d,%d]", type_str, node.job, node.index);
        return buf;
    };

    // ---- 辅助：资源名 ----
    auto resName = [](const CPNode& node) -> string {
        if (node.type == CPNodeType::OPERATION) {
            char buf[16];
            snprintf(buf, sizeof(buf), "机器%d", node.resource);
            return buf;
        } else {
            char buf[16];
            snprintf(buf, sizeof(buf), "车辆%d", node.resource);
            return buf;
        }
    };

    // ---- 路径打印 ----
    printf("\n--- 关键路径 (source -> sink) ---\n");
    for (int i = 0; i < n; ++i) {
        const CPNode& node = result.path[i];
        const char* type_str;
        switch (node.type) {
            case CPNodeType::OPERATION:        type_str = "O"; break;
            case CPNodeType::TRANSPORT_EMPTY:  type_str = "E"; break;
            case CPNodeType::TRANSPORT_LOADED: type_str = "L"; break;
            default:                           type_str = "?"; break;
        }

        printf("[%2d] %s j=%-2d d=%-2d %-6s start=%4d end=%4d",
            i, type_str, node.job, node.index,
            resName(node).c_str(), node.start, node.end);

        if (i == 0) {
            printf("   <- 源点 (start=0)\n");
        } else if (i == n - 1) {
            printf("   <- makespan 终点\n");
        } else {
            const CPNode& prev = result.path[i - 1];
            printf("   (前驱 %s.end=%d == 本节点.start)\n",
                nodeLabel(prev).c_str(), prev.end);
        }
    }

    // ---- 机器关键块 ----
    if (!result.machine_blocks.empty()) {
        printf("\n--- 机器关键块 ---\n");
        for (size_t bi = 0; bi < result.machine_blocks.size(); ++bi) {
            const CriticalBlock& blk = result.machine_blocks[bi];
            const char* pos_str = "???";
            switch (blk.position) {
                case BlockPosition::FIRST:    pos_str = "首块(FIRST)";    break;
                case BlockPosition::INTERNAL: pos_str = "内部(INTERNAL)"; break;
                case BlockPosition::LAST:     pos_str = "尾块(LAST)";     break;
            }
            printf("块%d: 机器%d, 位置=%s, 节点数=%d, path下标=[%d,%d]\n",
                static_cast<int>(bi + 1), blk.resource_id, pos_str,
                blk.size(),
                blk.path_indices.front(), blk.path_indices.back());
            printf("  节点: ");
            for (int idx : blk.path_indices) {
                if (idx >= 0 && idx < n) {
                    printf("%s ", nodeLabel(result.path[idx]).c_str());
                }
            }
            printf("\n");
        }
    }

    // ---- 车辆关键块 ----
    if (!result.vehicle_blocks.empty()) {
        printf("\n--- 车辆关键块 ---\n");
        for (size_t bi = 0; bi < result.vehicle_blocks.size(); ++bi) {
            const CriticalBlock& blk = result.vehicle_blocks[bi];
            const char* pos_str = "???";
            switch (blk.position) {
                case BlockPosition::FIRST:    pos_str = "首块(FIRST)";    break;
                case BlockPosition::INTERNAL: pos_str = "内部(INTERNAL)"; break;
                case BlockPosition::LAST:     pos_str = "尾块(LAST)";     break;
            }
            printf("块%d: 车辆%d, 位置=%s, 节点数=%d, path下标=[%d,%d]\n",
                static_cast<int>(bi + 1), blk.resource_id, pos_str,
                blk.size(),
                blk.path_indices.front(), blk.path_indices.back());
            printf("  节点: ");
            for (int idx : blk.path_indices) {
                if (idx >= 0 && idx < n) {
                    printf("%s ", nodeLabel(result.path[idx]).c_str());
                }
            }
            printf("\n");
        }
    }

    // ---- 自洽性验证 ----
    printf("\n--- 自洽性验证 ---\n");
    int gap_count = 0;
    for (int i = 0; i < n - 1; ++i) {
        const CPNode& prev = result.path[i];
        const CPNode& curr = result.path[i + 1];
        if (prev.end == curr.start) {
            printf("[OK] 边 [%2d->%2d]: %s.end(%d) == %s.start(%d)  OK\n",
                i, i + 1,
                nodeLabel(prev).c_str(), prev.end,
                nodeLabel(curr).c_str(), curr.start);
        } else {
            printf("[FAIL] 边 [%2d->%2d]: %s.end(%d) != %s.start(%d)  gap=%d\n",
                i, i + 1,
                nodeLabel(prev).c_str(), prev.end,
                nodeLabel(curr).c_str(), curr.start,
                curr.start - prev.end);
            ++gap_count;
        }
    }

    if (gap_count == 0) {
        printf("\n总计 %d 条边，全部紧致 (zero slack) [OK]\n", n - 1);
    } else {
        printf("\n总计 %d 条边，%d 条存在松弛 [FAIL]\n", n - 1, gap_count);
    }

    printf("=========================================\n\n");
}

// ============================================================
//  generateGanttWithCriticalPath
//
//  生成带关键路径高亮的 HTML 甘特图。
//  关键路径上的工序和运输以深红色 (#C62828) 加边框显示，
//  非关键路径保持原色（工序绿、运输蓝、空载橙）。
//  若 cp.valid == false 则退化为普通甘特图。
// ============================================================
void generateGanttWithCriticalPath(
    const Chromosome& chromosome,
    const std::vector<GONGJIAN>& jobs,
    const yunshutime& yt,
    const char* filename,
    const CriticalPathResult& cp)
{
    bool is_hk = (yt.suanlileibei == "HK");

    // ---- 构建关键路径节点集合 (type_int, job, index) ----
    set<tuple<int, int, int>> cp_set;
    if (cp.valid) {
        for (const auto& node : cp.path) {
            cp_set.insert({static_cast<int>(node.type), node.job, node.index});
        }
    }

    FILE* f = nullptr;
    if (fopen_s(&f, filename, "wb") != 0 || !f) {
        cerr << "甘特图文件创建失败！" << endl;
        return;
    }
    unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    fwrite(bom, 1, 3, f);

    struct Task { int start, end, job; string label, type; };
    map<int, vector<Task>> vehicle_tasks;
    map<int, vector<Task>> machine_tasks;

    for (int j = 0; j < static_cast<int>(jobs.size()); ++j) {
        if (j >= static_cast<int>(chromosome.vehicle_assignment.size())) continue;
        int n_ops = static_cast<int>(jobs[j].gongxu_set.size());
        int n_trans = is_hk ? n_ops : n_ops + 1;

        for (int k = 0; k < n_trans; ++k) {
            if (k >= static_cast<int>(chromosome.vehicle_assignment[j].size())) continue;
            int v_id = chromosome.vehicle_assignment[j][k];
            if (v_id < 1) continue;

            int from_loc = (k == 0) ? 0 : jobs[j].gongxu_set[k - 1].jiqi_id;
            int to_loc = (k == n_ops) ? 0 : jobs[j].gongxu_set[k].jiqi_id;

            // 分别判断空载和满载是否在关键路径上
            // type=1: TRANSPORT_EMPTY, type=2: TRANSPORT_LOADED
            bool empty_critical = (cp_set.count({1, j, k}) > 0);
            bool loaded_critical = (cp_set.count({2, j, k}) > 0);

            // 空载段 — 缩写: E=空载, M=机床, LU=仓库, dep=虚拟depot
            int es = chromosome.empty_start[j][k];
            int ee = chromosome.empty_end[j][k];
            if (ee > es) {
                char buf[128];
                if (from_loc == 0 && !is_hk)
                    sprintf_s(buf, "E→LU");
                else if (from_loc == 0)
                    sprintf_s(buf, "E→M%d", to_loc);
                else
                    sprintf_s(buf, "E→M%d", from_loc);
                vehicle_tasks[v_id].push_back(
                    { es, ee, 0, buf, empty_critical ? "empty_cp" : "empty" });
            }

            // 满载段 — 缩写: M=机床, LU=仓库, dep=虚拟depot
            int ts = chromosome.trans_start[j][k];
            int te = chromosome.trans_end[j][k];
            if (te > ts) {
                char buf[128];
                if (!is_hk) {
                    if (from_loc == 0)
                        sprintf_s(buf, "J%d LU→M%d", j + 1, to_loc);
                    else if (to_loc == 0)
                        sprintf_s(buf, "J%d M%d→LU", j + 1, from_loc);
                    else
                        sprintf_s(buf, "J%d M%d→M%d", j + 1, from_loc, to_loc);
                } else {
                    if (from_loc == 0)
                        sprintf_s(buf, "J%d dep→M%d", j + 1, to_loc);
                    else
                        sprintf_s(buf, "J%d M%d→M%d", j + 1, from_loc, to_loc);
                }
                vehicle_tasks[v_id].push_back(
                    { ts, te, j + 1, buf, loaded_critical ? "load_cp" : "load" });
            }

            // 工序（仅生产工序，k < n_ops）
            if (k < n_ops && k < static_cast<int>(chromosome.op_end[j].size())) {
                int m_id = jobs[j].gongxu_set[k].jiqi_id;
                char buf[64];
                sprintf_s(buf, "J%d-O%d", j + 1, k + 1);
                bool is_critical = (cp_set.count({0, j, k}) > 0);
                machine_tasks[m_id].push_back(
                    { chromosome.op_start[j][k], chromosome.op_end[j][k],
                      j + 1, buf, is_critical ? "op_cp" : "op" });
            }
        }
    }

    // ---- HTML 头部 ----
    fprintf(f, "<!DOCTYPE html><html><head><meta charset='utf-8'>");
    if (cp.valid) {
        fprintf(f, "<title>甘特图（含关键路径高亮）</title>");
    } else {
        fprintf(f, "<title>甘特图（关键路径无效）</title>");
    }
    fprintf(f, "<style>");
    fprintf(f, "body{font-family:Microsoft YaHei,Arial;margin:20px}"
        ".row{height:40px;position:relative;border-bottom:1px solid #ddd}"
        ".label-col{width:100px;display:inline-block;font-weight:bold;line-height:40px}"
        ".bar{height:20px;position:absolute;top:10px}"
        ".op{background:#4CAF50}"
        ".op-critical{background:#C62828;border:2px solid #B71C1C}"
        ".trans{background:#2196F3}"
        ".trans-critical{background:#C62828;border:2px solid #B71C1C}"
        ".empty{background:#FFA726;opacity:0.7}"
        ".empty-critical{background:#C62828;border:2px solid #B71C1C;opacity:0.9}"
        ".txt{position:absolute;top:-16px;left:2px;color:#000;font-size:6px;"
        "white-space:nowrap;font-weight:bold}");
    fprintf(f, "</style></head><body>");

    if (cp.valid) {
        fprintf(f, "<h2>调度甘特图（含关键路径高亮） (Makespan: %d)</h2>", chromosome.fitness);
    } else {
        fprintf(f, "<h2>调度甘特图（关键路径无效） (Makespan: %d)</h2>", chromosome.fitness);
    }

    // ---- 构建资源行号映射 (用于关键路径跨行 SVG 连线) ----
    int cp_row_idx = 0;
    map<int, int> cp_vehicle_row;
    for (auto& vt : vehicle_tasks) { cp_vehicle_row[vt.first] = cp_row_idx++; }
    map<int, int> cp_machine_row;
    for (auto& mt : machine_tasks) { cp_machine_row[mt.first] = cp_row_idx++; }
    int cp_total_rows = cp_row_idx;
    int cp_chart_w = 100 + chromosome.fitness * 5 + 30;

    // 辅助：返回节点所在资源行的 Y 中心坐标 (px)
    auto cp_node_y = [&](const CPNode& node) -> int {
        int r = -1;
        if (node.type == CPNodeType::TRANSPORT_EMPTY || node.type == CPNodeType::TRANSPORT_LOADED) {
            auto it = cp_vehicle_row.find(node.resource);
            if (it != cp_vehicle_row.end()) r = it->second;
        } else {
            auto it = cp_machine_row.find(node.resource);
            if (it != cp_machine_row.end()) r = it->second;
        }
        return (r >= 0) ? r * 40 + 20 : -1;
    };

    fprintf(f, "<div style='position:relative'>");  // 容器：供 SVG 绝对定位

    // ---- 车辆行 ----
    for (auto& vt : vehicle_tasks) {
        fprintf(f, "<div class='row'><span class='label-col'>车辆 %d</span>", vt.first);
        for (auto& t : vt.second) {
            const char* cls = nullptr;
            if (t.type == "empty") cls = "empty";
            else if (t.type == "empty_cp") cls = "empty-critical";
            else if (t.type == "load_cp") cls = "trans-critical";
            else cls = "trans";
            fprintf(f, "<div class='bar %s' style='left:%dpx;width:%dpx'>"
                "<span class='txt'>%s</span></div>",
                cls, 100 + t.start * 5, (t.end - t.start) * 5, t.label.c_str());
        }
        fprintf(f, "</div>");
    }

    // ---- 机器行 ----
    for (auto& mt : machine_tasks) {
        fprintf(f, "<div class='row'><span class='label-col'>机床 %d</span>", mt.first);
        for (auto& t : mt.second) {
            const char* cls = (t.type == "op_cp") ? "op-critical" : "op";
            fprintf(f, "<div class='bar %s' style='left:%dpx;width:%dpx'>"
                "<span class='txt'>%s</span></div>",
                cls, 100 + t.start * 5, (t.end - t.start) * 5, t.label.c_str());
        }
        fprintf(f, "</div>");
    }

    // ---- 关键路径跨行连线 (SVG 叠加层) ----
    // 关键路径相邻节点满足 prev.end == curr.start (零松弛)，
    // 因此连线为垂直线：X 坐标不变，Y 从上一节点所在行连到下一节点所在行。
    if (cp.valid && cp.path.size() >= 2) {
        fprintf(f, "<svg style='position:absolute;top:0;left:0;"
            "width:%dpx;height:%dpx;pointer-events:none;z-index:10' "
            "xmlns='http://www.w3.org/2000/svg'>",
            cp_chart_w, cp_total_rows * 40);
        // 箭头定义：深红色三角箭头
        fprintf(f, "<defs><marker id='cpArrow' markerWidth='8' markerHeight='6' "
            "refX='8' refY='3' orient='auto'>"
            "<polygon points='0 0, 8 3, 0 6' fill='#C62828'/>"
            "</marker></defs>");

        for (size_t pi = 0; pi + 1 < cp.path.size(); ++pi) {
            const CPNode& prev = cp.path[pi];
            const CPNode& curr = cp.path[pi + 1];

            int y1 = cp_node_y(prev);
            int y2 = cp_node_y(curr);
            if (y1 < 0 || y2 < 0 || y1 == y2) continue;  // 同行则跳过

            int x = 100 + prev.end * 5;  // prev.end == curr.start (关键路径性质)

            fprintf(f, "<line x1='%d' y1='%d' x2='%d' y2='%d' "
                "stroke='#C62828' stroke-width='2' stroke-dasharray='6,3' "
                "marker-end='url(#cpArrow)'/>",
                x, y1, x, y2);
        }
        fprintf(f, "</svg>");
    }

    fprintf(f, "</div>");  // 容器结束

    // ---- 图例 ----
    fprintf(f, "<div style='margin-top:20px;padding:10px;background:#f5f5f5;border-radius:5px'>");
    fprintf(f, "<b>图例：</b>");
    fprintf(f, "<span style='display:inline-block;width:16px;height:16px;background:#4CAF50;"
        "vertical-align:middle;margin:0 4px'></span> 非关键工序  ");
    fprintf(f, "<span style='display:inline-block;width:16px;height:16px;background:#C62828;"
        "vertical-align:middle;margin:0 4px;border:2px solid #B71C1C'></span> 关键路径工序  ");
    fprintf(f, "<span style='display:inline-block;width:16px;height:16px;background:#2196F3;"
        "vertical-align:middle;margin:0 4px'></span> 非关键运输  ");
    fprintf(f, "<span style='display:inline-block;width:16px;height:16px;background:#C62828;"
        "vertical-align:middle;margin:0 4px;border:2px solid #B71C1C'></span> 关键路径运输  ");
    fprintf(f, "<span style='display:inline-block;width:16px;height:16px;background:#C62828;"
        "vertical-align:middle;margin:0 4px;border:2px solid #B71C1C;opacity:0.9'></span> 关键路径空载  ");
    fprintf(f, "<span style='display:inline-block;width:16px;height:16px;background:#FFA726;"
        "vertical-align:middle;margin:0 4px;opacity:0.7'></span> 非关键空载");
    fprintf(f, "</div>");

    fprintf(f, "</body></html>");
    fclose(f);
}
