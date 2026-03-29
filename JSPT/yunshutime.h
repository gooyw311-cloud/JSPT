#pragma once
#include<vector>
#include<string>
class yunshutime
{
public:
	std::string suanlileibei;//算法类别
	std::vector<std::vector<int>>man;//man[i][j]表示i机器到j机器的满载搬运时间
	std::vector<std::vector<int>>kong;//kong[i][j]表示i机器到j机器的空载搬运时间
	std::vector<std::vector<int>>BU;//BU[i][j]表示i机器到j机器的搬运时间
	std::vector < std::vector<int>>man_hk;
};