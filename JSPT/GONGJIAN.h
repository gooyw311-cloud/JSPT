#pragma once
#include<vector>
#include"GONGXU.h"
class GONGJIAN
{
public:
	int id;//工件id
	std::vector<GONGXU>gongxu_set;//工件的工序集合
};