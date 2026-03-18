#pragma once

class GONGXU
{
public:
	int id;//工件id
	int op_index;//工序在工件中的序号
	int jiqi_id;//工序所在机器id
	int jiagong_time;//工序加工时间
	int man_time;// 满载运输时间
	int vehicle_id;// 负责将工件送往该机器的车辆id
};