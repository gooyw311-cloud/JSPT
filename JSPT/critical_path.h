#pragma once

#include <vector>
#include "Chromosome.h"
#include "GONGJIAN.h"
#include "yunshutime.h"

// ============================================================
// 关键路径分析 — 数据结构与接口
//
// 用途：对已解码的染色体 (calculate() 已执行) 进行关键路径分析，
//       识别机器关键块和车辆关键块，为 N8 邻域提供操作目标。
//
// 依赖关系回顾（Chromosome::calculate / schedule_transport）:
//   工序 O_{j,d} 的 start = max( trans_end[j][d],
//                               op_end[j][d-1],
//                               machine_free[m_id] )
//   运输 T_{j,d} 的 start = max( empty_end[j][d],
//                               op_end[j][d-1] )
//   空驶 E_{j,d} 的 start = vehicle_free[v_id]（上一运输任务结束时间）
//
// 关键路径：从 makespan 逆向追溯，每一步找出"决定"了当前节点
//           开始时间的那个（些）前驱节点，直到 time=0 的源点。
// ============================================================

// ---- 节点类型 ---------------------------------------------------------------
enum class CPNodeType {
    OPERATION,        // 加工工序
    TRANSPORT_EMPTY,  // 空载移动（车辆去 pickup 点）
    TRANSPORT_LOADED  // 满载移动（车辆运送工件）
};

// ---- 关键路径上的一个节点 ----------------------------------------------------
// type == OPERATION       → resource = 机器 id, start/end = op_start/op_end
// type == TRANSPORT_EMPTY → resource = 车辆 id, start/end = empty_start/empty_end
// type == TRANSPORT_LOADED→ resource = 车辆 id, start/end = trans_start/trans_end
struct CPNode {
    CPNodeType type;   // 工序 or 运输
    int job;           // 工件索引（0‑based，对应 jobs[j]）
    int index;         // 工序索引 d（0‑based）或运输段索引 d（0‑based）
    int resource;      // 所属资源：机器 id（工序）/ 车辆 id（运输）
    int start;         // 开始时间
    int end;           // 结束时间

    bool operator<(const CPNode& o) const {
        if (type != o.type) return static_cast<int>(type) < static_cast<int>(o.type);
        if (job != o.job) return job < o.job;
        return index < o.index;
    }
    bool operator==(const CPNode& o) const {
        return type == o.type && job == o.job && index == o.index;
    }
};

// ---- 关键块在路径上的位置分类 ------------------------------------------------
enum class BlockPosition {
    FIRST,     // 首块：块内第一个节点是关键路径的源点（无关键前驱）
    INTERNAL,  // 内部块：非首非尾
    LAST       // 尾块：块内最后一个节点是关键路径的终点 makespan（无关键后继）
};

// ---- 一个关键块 --------------------------------------------------------------
// 关键块 = 在关键路径上，连续通过同一资源（机器/车辆）的析取弧相连的一组节点。
// N8 邻域根据块的位置（首/内部/尾）决定允许的交换方式。
struct CriticalBlock {
    int resource_id;                // 机器 id 或车辆 id
    bool is_machine_block;          // true = 机器块，false = 车辆块
    std::vector<int> path_indices;  // 指向 CriticalPathResult::path 的下标，连续递增
    BlockPosition position;         // 首 / 内部 / 尾

    int size() const { return static_cast<int>(path_indices.size()); }
};

// ---- 关键路径分析结果 --------------------------------------------------------
struct CriticalPathResult {
    std::vector<CPNode> path;              // 从源点到终点的关键节点序列（单条路径）
    std::vector<CriticalBlock> machine_blocks; // 机器关键块列表
    std::vector<CriticalBlock> vehicle_blocks; // 车辆关键块列表
    bool valid;                            // 分析是否成功（calculate() 已执行过）

    CriticalPathResult() : valid(false) {}
};

// ---- 主接口 -----------------------------------------------------------------
// 前置条件：chromosome.calculate(yt, jobs) 已调用，时间数组已填充。
// 参数：
//   chromosome : 已解码的染色体
//   jobs       : 问题实例的工件集合
//   yt         : 运输时间数据（用于区分 HK / BU·SWV 模式）
// 返回：
//   关键路径分析结果；若 calculate() 未执行则 valid == false。
CriticalPathResult findCriticalPath(
    const Chromosome& chromosome,
    const std::vector<GONGJIAN>& jobs,
    const yunshutime& yt
);

// ---- 关键路径可视化与验证 ----------------------------------------------------
// 打印关键路径的文本追溯链（含自洽性验证），用于直观证明关键路径分析的正确性。
void printCriticalPathTrace(const CriticalPathResult& result);

// 生成带关键路径高亮的甘特图 HTML 文件。
// 关键路径上的工序和运输以深红色高亮并加边框，非关键路径保持原色。
// 若 cp.valid == false 则退化为普通甘特图（不加高亮）。
void generateGanttWithCriticalPath(
    const Chromosome& chromosome,
    const std::vector<GONGJIAN>& jobs,
    const yunshutime& yt,
    const char* filename,
    const CriticalPathResult& cp
);
