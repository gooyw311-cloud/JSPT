#pragma once
#include <vector>
#include "yunshutime.h"
#include "GONGJIAN.h"
class Chromosome {
public:

    Chromosome();
    ~Chromosome();
	Chromosome(const Chromosome& other);
	Chromosome& operator=(const Chromosome& other);
    int fitness = 0;
    // 第一层：资源分配 [工件ID][第几次运输] -> 车辆ID
    std::vector<std::vector<int>> vehicle_assignment; 

    // 第二层：加工顺序 (OS)
    std::vector<int> operation_sequence;

    // 第三层：运输顺序 (TS)
    std::vector<int> transport_sequence;
    std::vector<std::vector<int>>op_end;
    std::vector<std::vector<int>>trans_end;
    void schedule_transport(
        int job_id, int d,
        const std::vector<GONGJIAN>& jobs,
        const yunshutime& yt,
        std::vector<int>& vehicle_free,
        std::vector<int>& vehicle_pos,
        std::vector<int>& trans_done
    );
    void calculate(const yunshutime& yt, const std::vector<GONGJIAN>& jobs);
	void print() const;
};