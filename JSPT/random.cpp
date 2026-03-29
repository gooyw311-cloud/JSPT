#include "random.h"
#include <algorithm>
#include <random>
#include <ctime>

Chromosome generate_random_chromosome(
    const std::vector<GONGJIAN>& jobs,
    int num_vehicles,
    const yunshutime& yt)
{
    Chromosome chro;
    static std::mt19937 g(static_cast<unsigned int>(time(0)));

    bool is_hk = (yt.suanlileibei == "HK");
    int num_jobs = (int)jobs.size();
    chro.vehicle_assignment.resize(num_jobs);

    // ── 第一层：车辆分配 ──────────────────────────────────────────────────────
    // BU/SWV : 每工件有 n_ops + 1 段运输（含返回 LU）
    // HK     : 每工件有 n_ops     段运输（无返回 LU）
    for (const auto& job : jobs)
    {
        int n_ops = (int)job.gongxu_set.size();
        int n_trans = is_hk ? n_ops : n_ops + 1;

        chro.vehicle_assignment[job.id - 1].clear();
        for (int k = 0; k < n_trans; ++k)
            chro.vehicle_assignment[job.id - 1].push_back((g() % num_vehicles) + 1);
    }

    // ── 第二层：工序序列（OS）────────────────────────────────────────────────
    // BU/SWV : 每工件 n_ops 个生产条目 + 1 个返回 LU 条目，共 n_ops+1 次出现
    // HK     : 每工件 n_ops 个生产条目，不插入返回 LU 条目

    // 先放入所有生产条目并打乱
    for (const auto& job : jobs)
    {
        int n_ops = (int)job.gongxu_set.size();
        for (int k = 0; k < n_ops; ++k)
            chro.operation_sequence.push_back(job.id);
    }
    std::shuffle(chro.operation_sequence.begin(), chro.operation_sequence.end(), g);

    // BU/SWV 额外步骤：为每个工件插入一个"返回 LU"条目
    // 插入位置必须在该工件最后一道生产工序之后（保证可行性）
    if (!is_hk)
    {
        for (const auto& job : jobs)
        {
            int n_ops = (int)job.gongxu_set.size();
            int seq_size = (int)chro.operation_sequence.size();

            // 找到 job.id 第 n_ops 次出现的下标（最后一道生产工序位置）
            int last_pos = 0, count = 0;
            for (int i = 0; i < seq_size; ++i)
            {
                if (chro.operation_sequence[i] == job.id)
                    if (++count == n_ops) { last_pos = i; break; }
            }

            // 在 [last_pos+1, seq_size] 范围内随机选插入位置
            int range = seq_size - last_pos;   // 可选位置数 ≥ 1
            int insert_pos = last_pos + 1 + (int)(g() % range);
            chro.operation_sequence.insert(
                chro.operation_sequence.begin() + insert_pos, job.id);
        }
    }

    chro.transport_sequence.clear();
    return chro;
}