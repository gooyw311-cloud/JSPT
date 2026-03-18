#include "random.h"
#include <algorithm>
#include <random>
#include <ctime>


Chromosome generate_random_chromosome(const std::vector<GONGJIAN>& jobs, int num_vehicles) {
    Chromosome chro;
    static std::mt19937 g(static_cast<unsigned int>(time(0)));

    chro.vehicle_assignment.resize(jobs.size());

    for (const auto& job : jobs) {
        int n_ops = (int)job.gongxu_set.size();
        int n_trans = n_ops + 1; // 运输任务 = 工序数 + 1 [cite: 1, 2, 4]

        // 第一层：为该工件的每一次运输随机选一辆车
        for (int j = 0; j < n_trans; ++j) {
            chro.vehicle_assignment[job.id - 1].push_back((g() % num_vehicles) + 1);
        }

        // 第二层：放入工件ID，次数等于工序数 [cite: 3]
        for (int j = 0; j < n_ops; ++j) {
            chro.operation_sequence.push_back(job.id);
        }

        // 第三层：放入工件ID，次数等于运输数 
        for (int j = 0; j < n_trans; ++j) {
            chro.transport_sequence.push_back(job.id);
        }
    }

    std::shuffle(chro.operation_sequence.begin(), chro.operation_sequence.end(), g);
    std::shuffle(chro.transport_sequence.begin(), chro.transport_sequence.end(), g);

    return chro;
}