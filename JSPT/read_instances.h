#pragma once
#include <string>
#include <vector>
#include "GONGJIAN.h"      // 确保 GONGJIAN 类型已定义
#include "yunshutime.h"    // 确保 yunshutime 类型已定义

void read_instances(const std::string& filename, 
                    std::vector<GONGJIAN>& machines, 
                    yunshutime& time);