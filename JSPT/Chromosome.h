#pragma once
#include <vector>

class Chromosome {
public:
    int fitness = 0;
    // 第一层：资源分配 [工件ID][第几次运输] -> 车辆ID
    std::vector<std::vector<int>> vehicle_assignment; 

    // 第二层：加工顺序 (OS)
    std::vector<int> operation_sequence;

    // 第三层：运输顺序 (TS)
    std::vector<int> transport_sequence;
    void calculate() {}
    
};