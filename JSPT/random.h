#pragma once
#include <vector>
#include "Chromosome.h"
#include "GONGJIAN.h" // 需要用到工件定义

// 声明随机生成函数
// num_vehicles: 系统的总车辆数
Chromosome generate_random_chromosome(const std::vector<GONGJIAN>& jobs, int num_vehicles);