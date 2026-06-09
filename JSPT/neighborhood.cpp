#include "neighborhood.h"
#include <set>
#include <map>
#include <algorithm>
#include <cstdio>

using namespace std;

namespace {

// ============================================================
//  在 operation_sequence 中查找工序 O_{j,d} 的位置
//
//  CPNode{type=OPERATION, job=j, index=d} 对应 OS 中
//  job_id=j+1 的第 d+1 次出现。
//
//  例: OS = [1, 3, 2, 1, 3, 2]
//    job=0 (id=1), d=0 → 扫描 OS，job_id=1 首次出现 → pos=0
//    job=0 (id=1), d=1 → 扫描 OS，job_id=1 第二次出现 → pos=3
//    job=2 (id=3), d=0 → 扫描 OS，job_id=3 首次出现 → pos=1
//
//  返回 OS 中的下标 (0-based)，理论上总能找到。
// ============================================================
int findOSPosition(const vector<int>& os, int job_id, int occur) {
    int count = 0;
    int n = static_cast<int>(os.size());
    for (int i = 0; i < n; ++i) {
        if (os[i] == job_id) {
            if (count == occur) return i;
            ++count;
        }
    }
    return -1;  // 防御：理论上不会到达（关键块节点必然在 OS 中存在）
}

}  // anonymous namespace

// ============================================================
//  generateN8Moves
//
//  遍历所有机器关键块，按 N8 规则收集工序交换对。
//  同一 swap 只保留一次（set 去重）。
//
//  N8 块位置规则:
//    FIRST    — 只交换块内最后两个工序 (k-2, k-1)
//    LAST     — 只交换块内前两个工序 (0, 1)
//    INTERNAL — 交换前两个 AND 最后两个工序
//   （k==2 时两对重合，set 自动去重）
// ============================================================
vector<N8Move> generateN8Moves(
    const Chromosome& chromosome,
    const CriticalPathResult& cp,
    const vector<GONGJIAN>& jobs)
{
    vector<N8Move> moves;

    // 前置条件检查
    if (!cp.valid || cp.machine_blocks.empty()) return moves;

    const auto& os = chromosome.operation_sequence;

    // 去重：sorted pair (min_pos, max_pos) → 已生成
    set<pair<int, int>> seen;

    for (const CriticalBlock& block : cp.machine_blocks) {
        int k = block.size();
        if (k < 2) continue;  // 防御（extractBlocks 已过滤，但保留安全性）

        // ---- 确定要交换的块内下标对 ----
        vector<pair<int, int>> swap_pairs;  // (idx_in_block_a, idx_in_block_b)

        switch (block.position) {
        case BlockPosition::FIRST:
            // 首块：只交换最后两个工序
            swap_pairs.push_back({ k - 2, k - 1 });
            break;

        case BlockPosition::LAST:
            // 尾块：只交换前两个工序
            swap_pairs.push_back({ 0, 1 });
            break;

        case BlockPosition::INTERNAL:
            // 内部块：交换前两个 和 最后两个
            swap_pairs.push_back({ 0, 1 });      // 前两个
            swap_pairs.push_back({ k - 2, k - 1 });  // 后两个
            break;
        }

        // ---- 将块内下标映射为 OS 位置并生成 N8Move ----
        for (const auto& pr : swap_pairs) {
            int idx_a = pr.first;
            int idx_b = pr.second;

            // 从关键路径取对应 CPNode
            const CPNode& node_a = cp.path[block.path_indices[idx_a]];
            const CPNode& node_b = cp.path[block.path_indices[idx_b]];

            // 仅处理工序节点（防御：机器块中理论上全是 OPERATION）
            if (node_a.type != CPNodeType::OPERATION ||
                node_b.type != CPNodeType::OPERATION) {
                continue;
            }

            // CPNode → OS 位置映射
            int pos_a = findOSPosition(os, node_a.job + 1, node_a.index);
            int pos_b = findOSPosition(os, node_b.job + 1, node_b.index);

            if (pos_a < 0 || pos_b < 0) continue;  // 防御

            // 去重：sorted pair
            pair<int, int> key = {
                min(pos_a, pos_b),
                max(pos_a, pos_b)
            };

            if (seen.insert(key).second) {
                moves.push_back({ pos_a, pos_b, block.resource_id });
            }
        }
    }

    return moves;
}

// ============================================================
//  applyN8Move
//
//  拷贝染色体 → 交换 OS 中两个位置 → 重新解码。
//  operation_sequence 长度和各工件出现次数不变（纯粹排列交换），
//  因此无需额外可行性检查。
// ============================================================
Chromosome applyN8Move(
    const Chromosome& chromosome,
    const N8Move& move,
    const yunshutime& yt,
    const std::vector<GONGJIAN>& jobs)
{
    Chromosome neighbor = chromosome;  // 利用拷贝构造函数
    swap(neighbor.operation_sequence[move.pos1],
         neighbor.operation_sequence[move.pos2]);
    neighbor.calculate(yt, jobs);
    return neighbor;
}

// ============================================================
//  generateN8Neighbors
//
//  组合 generateN8Moves + applyN8Move，一步生成所有 N8 邻居。
//  适合测试调试场景，禁忌搜索主循环中建议分开调用以优化评估顺序。
// ============================================================
vector<Chromosome> generateN8Neighbors(
    const Chromosome& chromosome,
    const CriticalPathResult& cp,
    const vector<GONGJIAN>& jobs,
    const yunshutime& yt)
{
    vector<Chromosome> neighbors;
    vector<N8Move> moves = generateN8Moves(chromosome, cp, jobs);
    neighbors.reserve(moves.size());

    for (const N8Move& move : moves) {
        neighbors.push_back(applyN8Move(chromosome, move, yt, jobs));
    }

    return neighbors;
}

// ============================================================
//  verifyN8Detailed — 逐块逐 move 验证 N8 规则正确性
//
//  此函数是纯验证逻辑，不修改染色体。输出分为三个层次:
//   Level 1 — 块内容展示: 每个机器块的节点列表 + 位置分类
//   Level 2 — 交换对检查: N8 选中的 pair 是否在块内的正确位置
//   Level 3 — OS 完整性: 交换后各工件出现次数是否与原始一致
//
//  返回 0 = 全部通过, >0 = 发现问题数
// ============================================================
int verifyN8Detailed(
    const Chromosome& chromosome,
    const CriticalPathResult& cp,
    const vector<GONGJIAN>& jobs,
    const yunshutime& yt)
{
    using namespace std;

    int failures = 0;
    bool is_hk = (yt.suanlileibei == "HK");
    const auto& os = chromosome.operation_sequence;

    printf("\n");
    printf("################################################################\n");
    printf("##          N8 邻域详查 — 逐块逐 Move 验证报告               ##\n");
    printf("################################################################\n");
    printf("算例模式 : %s\n", is_hk ? "HK" : "BU/SWV");
    printf("makespan : %d\n", chromosome.fitness);
    printf("OS 长度  : %d\n", static_cast<int>(os.size()));
    printf("关键路径节点数 : %d\n", static_cast<int>(cp.path.size()));
    printf("机器关键块数   : %d\n", static_cast<int>(cp.machine_blocks.size()));
    printf("车辆关键块数   : %d\n", static_cast<int>(cp.vehicle_blocks.size()));

    // ---- 辅助: 节点标签 ----
    auto nodeLabel = [](const CPNode& node) -> string {
        char buf[32];
        const char* t = "?";
        switch (node.type) {
            case CPNodeType::OPERATION:        t = "O"; break;
            case CPNodeType::TRANSPORT_EMPTY:  t = "E"; break;
            case CPNodeType::TRANSPORT_LOADED: t = "L"; break;
        }
        snprintf(buf, sizeof(buf), "%s[J%d,d%d]", t, node.job + 1, node.index);
        return buf;
    };

    // ---- 辅助: 块位置字符串 ----
    auto posName = [](BlockPosition p) -> const char* {
        switch (p) {
            case BlockPosition::FIRST:    return "FIRST(首块)";
            case BlockPosition::LAST:     return "LAST(尾块)";
            case BlockPosition::INTERNAL: return "INTERNAL(内块)";
        }
        return "???";
    };

    // ================================================================
    //  Level 1 — 逐块展示内容
    // ================================================================
    printf("\n========== Level 1: 关键块内容展示 ==========\n");

    for (size_t bi = 0; bi < cp.machine_blocks.size(); ++bi) {
        const CriticalBlock& blk = cp.machine_blocks[bi];
        int k = blk.size();

        printf("\n--- 机器块 #%zu: 机器 %d, 长度=%d, %s ---\n",
            bi + 1, blk.resource_id, k, posName(blk.position));

        // 打印块内所有节点
        printf("  块内节点 (按关键路径顺序):\n");
        for (int i = 0; i < k; ++i) {
            int path_idx = blk.path_indices[i];
            if (path_idx < 0 || path_idx >= static_cast<int>(cp.path.size())) {
                printf("    [%d] ??? (path下标越界)\n", i);
                ++failures;
                continue;
            }
            const CPNode& node = cp.path[path_idx];
            int job_id = node.job + 1;  // 1-based
            int os_pos = -1;

            // 找该工序在 OS 中的位置
            {
                int cnt = 0;
                for (int p = 0; p < static_cast<int>(os.size()); ++p) {
                    if (os[p] == job_id) {
                        if (cnt == node.index) { os_pos = p; break; }
                        ++cnt;
                    }
                }
            }

            int proc_time = (node.index < static_cast<int>(jobs[node.job].gongxu_set.size()))
                ? jobs[node.job].gongxu_set[node.index].jiagong_time : -1;

            printf("    块内[%d] = path[%d] = %-12s 机器%d  start=%4d  end=%4d  "
                   "加工时长=%d  OS位置=%d\n",
                i, path_idx, nodeLabel(node).c_str(), node.resource,
                node.start, node.end, proc_time, os_pos);

            if (os_pos < 0) {
                printf("      [ERROR] 找不到工序在 OS 中的位置!\n");
                ++failures;
            }
        }

        // ---- N8 规则说明 ----
        printf("  N8 交换规则 (k=%d, %s): ", k, posName(blk.position));
        switch (blk.position) {
        case BlockPosition::FIRST:
            printf("只交换最后两个 → 块内下标 (%d, %d)\n", k - 2, k - 1);
            break;
        case BlockPosition::LAST:
            printf("只交换前两个   → 块内下标 (0, 1)\n");
            break;
        case BlockPosition::INTERNAL:
            printf("交换前两个 (%d,%d) 和 后两个 (%d,%d)\n",
                0, 1, k - 2, k - 1);
            break;
        }
    }

    // ================================================================
    //  Level 2 — N8 Move 生成与规则核对
    // ================================================================
    printf("\n========== Level 2: N8 Move 逐条核对 ==========\n");

    vector<N8Move> moves = generateN8Moves(chromosome, cp, jobs);
    printf("共生成 %d 条 N8 move\n", static_cast<int>(moves.size()));

    // ---- 去重检查 ----
    {
        set<pair<int, int>> seen;
        int dup_count = 0;
        for (const auto& m : moves) {
            pair<int, int> key = { min(m.pos1, m.pos2), max(m.pos1, m.pos2) };
            if (!seen.insert(key).second) {
                printf("  [FAIL] 重复 move: swap OS[%d]<->OS[%d]\n", m.pos1, m.pos2);
                ++dup_count;
            }
        }
        if (dup_count == 0) {
            printf("[OK] 去重检查: 无重复 move\n");
        } else {
            printf("[FAIL] 去重检查: 发现 %d 条重复\n", dup_count);
            failures += dup_count;
        }
    }

    // ---- 逐条 move 核对 ----
    for (size_t mi = 0; mi < moves.size(); ++mi) {
        const N8Move& m = moves[mi];
        printf("\n  Move #%zu: swap OS[%d]<->OS[%d] (机器%d)\n",
            mi + 1, m.pos1, m.pos2, m.block_resource);

        // ① 定位工序
        int job_a = os[m.pos1];
        int job_b = os[m.pos2];

        // 查找 job_a 在 OS 中 m.pos1 位置是第几次出现
        int occur_a = 0;
        for (int p = 0; p < m.pos1; ++p) {
            if (os[p] == job_a) ++occur_a;
        }
        int occur_b = 0;
        for (int p = 0; p < m.pos2; ++p) {
            if (os[p] == job_b) ++occur_b;
        }

        int j_a = job_a - 1;  // 0-based job index
        int j_b = job_b - 1;
        int d_a = occur_a;    // 0-based operation index
        int d_b = occur_b;

        int m_a = (d_a < static_cast<int>(jobs[j_a].gongxu_set.size()))
            ? jobs[j_a].gongxu_set[d_a].jiqi_id : -1;
        int m_b = (d_b < static_cast<int>(jobs[j_b].gongxu_set.size()))
            ? jobs[j_b].gongxu_set[d_b].jiqi_id : -1;

        printf("    工序 A: Job%d-O%d (OS[%d]) → 机器%d\n", job_a, d_a + 1, m.pos1, m_a);
        printf("    工序 B: Job%d-O%d (OS[%d]) → 机器%d\n", job_b, d_b + 1, m.pos2, m_b);

        // ② 检查两工序是否同机器
        if (m_a != m_b || m_a != m.block_resource) {
            printf("    [FAIL] 机器不匹配: A→机器%d, B→机器%d, block→机器%d\n",
                m_a, m_b, m.block_resource);
            ++failures;
            continue;
        }
        printf("    [OK] 两工序同属机器 %d\n", m_a);

        // ③ 找到两工序所在的关键块，验证 N8 规则
        bool found_in_block = false;
        for (const CriticalBlock& blk : cp.machine_blocks) {
            if (blk.resource_id != m.block_resource) continue;

            int k = blk.size();
            // 在该块中搜索 CPNode 匹配
            int pos_in_block_a = -1, pos_in_block_b = -1;
            for (int i = 0; i < k; ++i) {
                const CPNode& node = cp.path[blk.path_indices[i]];
                if (node.type != CPNodeType::OPERATION) continue;
                if (node.job == j_a && node.index == d_a) pos_in_block_a = i;
                if (node.job == j_b && node.index == d_b) pos_in_block_b = i;
            }

            if (pos_in_block_a < 0 || pos_in_block_b < 0) continue;  // 不在这个块

            // 确保 pos_in_block_a < pos_in_block_b（路径顺序）
            if (pos_in_block_a > pos_in_block_b) swap(pos_in_block_a, pos_in_block_b);

            printf("    在机器块中找到: 节点在块内下标 A=%d, B=%d, 块长度=%d, %s\n",
                pos_in_block_a, pos_in_block_b, k, posName(blk.position));

            // ④ 验证这两个位置是否连续
            bool is_adjacent = (pos_in_block_b == pos_in_block_a + 1);
            printf("    块内相邻? %s\n", is_adjacent ? "是" : "否(非直接相邻，检查合理性...)");

            // ⑤ 验证 N8 位置规则
            bool rule_ok = false;
            const char* rule_desc = "";
            switch (blk.position) {
            case BlockPosition::FIRST:
                // 首块: 只允许交换最后两个 (k-2, k-1)
                rule_ok = (pos_in_block_a == k - 2 && pos_in_block_b == k - 1);
                rule_desc = "FIRST块规则: 必须为块内最后两个 (k-2, k-1)";
                break;
            case BlockPosition::LAST:
                // 尾块: 只允许交换前两个 (0, 1)
                rule_ok = (pos_in_block_a == 0 && pos_in_block_b == 1);
                rule_desc = "LAST块规则: 必须为块内前两个 (0, 1)";
                break;
            case BlockPosition::INTERNAL:
                // 内部块: 允许交换前两个(0,1) 或 后两个(k-2,k-1)
                rule_ok = (pos_in_block_a == 0 && pos_in_block_b == 1)
                       || (pos_in_block_a == k - 2 && pos_in_block_b == k - 1);
                rule_desc = "INTERNAL块规则: 必须为 (0,1) 或 (k-2, k-1)";
                break;
            }

            if (rule_ok) {
                printf("    [OK] %s ✓\n", rule_desc);
            } else {
                printf("    [FAIL] %s\n", rule_desc);
                printf("           实际交换位置: 块内[%d]<->块内[%d]\n",
                    pos_in_block_a, pos_in_block_b);
                ++failures;
            }

            found_in_block = true;
            break;
        }

        if (!found_in_block) {
            printf("    [FAIL] 该 swap 对应的两道工序不在同一关键块中!\n");
            ++failures;
        }
    }

    // ================================================================
    //  Level 3 — OS 完整性检查 (交换后工件出现次数不变)
    // ================================================================
    printf("\n========== Level 3: OS 完整性检查 ==========\n");

    // 统计原始 OS 中各工件出现次数
    map<int, int> orig_counts;
    for (int id : os) orig_counts[id]++;

    // 对每个邻居检查
    auto neighbors = generateN8Neighbors(chromosome, cp, jobs, yt);
    bool os_integrity_ok = true;
    for (size_t ni = 0; ni < neighbors.size(); ++ni) {
        const auto& nb = neighbors[ni];
        map<int, int> nb_counts;
        for (int id : nb.operation_sequence) nb_counts[id]++;

        if (nb_counts != orig_counts) {
            printf("  [FAIL] 邻居 #%zu OS 工件出现次数与原始解不一致!\n", ni + 1);
            os_integrity_ok = false;
            ++failures;
        }

        if (nb.operation_sequence.empty() || nb.op_end.empty()) {
            printf("  [FAIL] 邻居 #%zu 解码结果为空!\n", ni + 1);
            os_integrity_ok = false;
            ++failures;
        }
    }
    if (os_integrity_ok) {
        printf("  [OK] 全部 %zu 个邻居 OS 完整性检查通过\n", neighbors.size());
    }

    // ================================================================
    //  Level 4 — 汇总
    // ================================================================
    printf("\n========== 验证汇总 ==========\n");
    if (failures == 0) {
        printf("结论: 全部检查通过 ✓ — N8 邻域实现正确\n");
    } else {
        printf("结论: 发现 %d 个问题 ✗ — 需要修复\n", failures);
    }
    printf("#################################################################\n\n");

    return failures;
}
