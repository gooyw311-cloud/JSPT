#include"Chromosome.h"
#include"yunshutime.h"
#include"random.h"
#include"GONGXU.h"
#include<map>
#include<iostream>
Chromosome::Chromosome() 
{
	fitness = 0;
	vehicle_assignment.clear();
	operation_sequence.clear();
	transport_sequence.clear();
}
Chromosome::~Chromosome() 
{
	vehicle_assignment.clear();
	operation_sequence.clear();
	transport_sequence.clear();
}
Chromosome::Chromosome(const Chromosome& other) 
{
	fitness = other.fitness;
	vehicle_assignment = other.vehicle_assignment;
	operation_sequence = other.operation_sequence;
	transport_sequence = other.transport_sequence;
}
Chromosome& Chromosome::operator=(const Chromosome& other) 
{
	if (this != &other) {
		fitness = other.fitness;
		vehicle_assignment = other.vehicle_assignment;
		operation_sequence = other.operation_sequence;
		transport_sequence = other.transport_sequence;
	}
	return *this;
}
void Chromosome::schedule_transport(
	int job_id, int d,
	const std::vector<GONGJIAN>& jobs,
	const yunshutime& yt,
	std::vector<int>& vehicle_free,
	std::vector<int>& vehicle_pos,
	std::vector<int>& trans_done)
{
	int j = job_id - 1;
	int n_ops = (int)jobs[j].gongxu_set.size();
	int v_id = vehicle_assignment[j][d];
	int from_loc = (d == 0) ? 0 : jobs[j].gongxu_set[d - 1].jiqi_id;
	int to_loc = (d == n_ops) ? 0 : jobs[j].gongxu_set[d].jiqi_id;
	int t_vehicle_arrive = vehicle_free[v_id] + yt.kong[vehicle_pos[v_id]][from_loc];
	int t_ready = (d == 0) ? 0 : op_end[j][d - 1];
	int t_start = std::max(t_vehicle_arrive, t_ready);
	int t_delivery = t_start + yt.man[from_loc][to_loc];
	trans_end[j][d] = t_delivery;
	vehicle_free[v_id] = t_delivery;
	vehicle_pos[v_id] = to_loc;
	trans_done[j]++;
}
void Chromosome::calculate(const yunshutime& yt,const std::vector<GONGJIAN>& jobs) 
{
	int time = 0;
	int num_jobs = (int)jobs.size();
	int num_vehicles = 0;
	for (const auto& va : vehicle_assignment) {
		for (int v : va) {
			if (v > num_vehicles) num_vehicles = v;
		}
	}
	std::vector<int>vehicle_free(num_vehicles + 1, 0); // 车辆空闲时间
	std::vector<int>vehicle_pos(num_vehicles + 1, 0); // 车辆当前位置
	int num_locs = (int)yt.man.size();
	std::vector<int>machine_free(num_locs, 0); // 机器空闲时间
	op_end.resize(num_jobs);
	trans_end.resize(num_jobs);
	for (int i = 0; i < num_jobs; ++i)
	{
		int n_ops_job = (int)jobs[i].gongxu_set.size();
		op_end[i].resize(n_ops_job, 0);
		trans_end[i].resize(n_ops_job + 1, 0);
	}
	std::vector<int>op_done(num_jobs, 0); // 记录每个工件当前进行到第几道工序
	std::vector<int>trans_done(num_jobs, 0); // 记录每个工件当前进行到第几次运输
	std::map<int, int>op_counter; 
	for (int id : operation_sequence)
	{
		int d = op_counter[id]++;
		int j = id - 1;
		while (trans_done[j] <=d)
		{
			int td = trans_done[j];
			this->schedule_transport(id, td, jobs, yt, vehicle_free, vehicle_pos, trans_done);
		}
		int machine_id = jobs[j].gongxu_set[d].jiqi_id;
		int process_time = jobs[j].gongxu_set[d].jiagong_time;
		int t_arrive = trans_end[j][d];
		int t_ready = (d == 0) ? 0 : op_end[j][d - 1];
		int t_start = std::max({ t_arrive, t_ready, machine_free[machine_id] });
		int t_finish = t_start + process_time;
		op_end[j][d] = t_finish;
		machine_free[machine_id] = t_finish;
		op_done[j]++;
	}
	for (int i = 0; i < num_jobs; ++i)
	{
		int n_ops_job = (int)jobs[i].gongxu_set.size();
		while (trans_done[i] <= n_ops_job)
		{
			int td = trans_done[i];
			this->schedule_transport(i + 1,td, jobs, yt, vehicle_free, vehicle_pos, trans_done);
		}
	}
	int makespan = 0;
	for (int i = 0; i < num_jobs; ++i)
	{
		int n_ops_job = (int)jobs[i].gongxu_set.size();
		makespan = std::max(makespan,trans_end[i][n_ops_job]);
	}
	fitness = makespan;
}
void Chromosome::print() const 
{
	for (int j = 0; j < op_end.size(); j++) {
		for (int k = 0; k < op_end[j].size(); k++) {
			printf("工件%d 工序%d 结束时间：%d\n", j + 1, k + 1, op_end[j][k]);
		}
	}
	for (int j = 0; j <trans_end.size(); j++) {
		for (int k = 0; k < trans_end[j].size(); k++) {
			printf("工件%d 运输%d 结束时间：%d\n", j + 1, k, trans_end[j][k]);
		}
	}
}
