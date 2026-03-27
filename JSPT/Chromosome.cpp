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
	int t_empty_start = vehicle_free[v_id];
	int t_vehicle_arrive = vehicle_free[v_id] + yt.kong[vehicle_pos[v_id]][from_loc];
	int t_ready = (d == 0) ? 0 : op_end[j][d - 1];
	int t_start = std::max(t_vehicle_arrive, t_ready);
	int t_delivery = t_start + yt.man[from_loc][to_loc];
	empty_start[j][d] = t_empty_start;
	empty_end[j][d] = t_vehicle_arrive;
	trans_start[j][d] = t_start;
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
	op_start.resize(num_jobs);
	trans_start.resize(num_jobs);
	empty_start.resize(num_jobs);
	empty_end.resize(num_jobs);
	for (int i = 0; i < num_jobs; ++i)
	{
		int n_ops_job = (int)jobs[i].gongxu_set.size();
		op_end[i].resize(n_ops_job, 0);
		trans_end[i].resize(n_ops_job + 1, 0);
		op_start[i].resize(n_ops_job, 0);
		trans_start[i].resize(n_ops_job + 1, 0);
		empty_start[i].resize(n_ops_job + 1, 0);
		empty_end[i].resize(n_ops_job + 1, 0);
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
		op_start[j][d] = t_start;
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
void Chromosome::generate_gantt(const std::vector<GONGJIAN>& jobs, const yunshutime& yt, const char* filename) const
{
	FILE* f = nullptr;
	if (fopen_s(&f, filename, "wb") != 0 || !f) return;
	unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
	fwrite(bom, 1, 3, f);

	struct Task { int start, end, job; std::string label, type; };
	std::map<int, std::vector<Task>> vehicle_tasks;
	std::map<int, std::vector<Task>> machine_tasks;

	for (int j = 0; j < (int)jobs.size(); j++) {
		for (int k = 0; k < (int)trans_end[j].size(); k++) {
			int v_id = vehicle_assignment[j][k];
			int from_loc = (k == 0) ? 0 : jobs[j].gongxu_set[k - 1].jiqi_id;
			int to_loc = (k == (int)jobs[j].gongxu_set.size()) ? 0 : jobs[j].gongxu_set[k].jiqi_id;

			// 空载段：直接用 empty_start/empty_end
			int es = empty_start[j][k];
			int ee = empty_end[j][k];
			if (ee > es) {
				char buf[128];
				// 空载起点和终点：小车从 from_loc 出发的上一个位置我们只有 from_loc，
				// 空载终点就是 from_loc
				// 但我们不知道空载起点是哪个机器，只能记录"前往 from_loc"
				if (from_loc == 0) sprintf_s(buf, "空载→LU");
				else sprintf_s(buf, "空载→M%d", from_loc);
				vehicle_tasks[v_id].push_back({ es, ee, 0, buf, "empty" });
			}

			// 满载段
			int ts = trans_start[j][k];
			int te = trans_end[j][k];
			if (te > ts) {
				char buf[128];
				if (from_loc == 0) sprintf_s(buf, "J%d: LU→M%d", j + 1, to_loc);
				else if (to_loc == 0) sprintf_s(buf, "J%d: M%d→LU", j + 1, from_loc);
				else sprintf_s(buf, "J%d: M%d→M%d", j + 1, from_loc, to_loc);
				vehicle_tasks[v_id].push_back({ ts, te, j + 1, buf, "load" });
			}

			// 机器加工段
			if (k < (int)op_end[j].size()) {
				int m_id = jobs[j].gongxu_set[k].jiqi_id;
				char buf[64];
				sprintf_s(buf, "J%d-O%d", j + 1, k + 1);
				machine_tasks[m_id].push_back({ op_start[j][k], op_end[j][k], j + 1, buf, "op" });
			}
		}
	}

	fprintf(f, "<!DOCTYPE html><html><head><meta charset='utf-8'><title>甘特图</title><style>");
	fprintf(f, "body{font-family:Arial;margin:20px}.row{height:40px;position:relative;border-bottom:1px solid #ddd}");
	fprintf(f, ".label-col{width:80px;display:inline-block;font-weight:bold;line-height:40px}.bar{height:20px;position:absolute;top:10px}");
	fprintf(f, ".op{background:#4CAF50}.trans{background:#2196F3}.empty{background:#FFA726;opacity:0.7}");
	fprintf(f, ".txt{position:absolute;top:-18px;left:2px;color:#000;font-size:10px;white-space:nowrap;font-weight:bold}");
	fprintf(f, "</style></head><body><h2>调度甘特图 (Makespan: %d)</h2>", fitness);

	for (auto& vt : vehicle_tasks) {
		int v_id = vt.first;
		auto& tasks = vt.second;
		fprintf(f, "<div class='row'><span class='label-col'>车辆 %d</span>", v_id);
		for (auto& t : tasks) {
			const char* cls = (t.type == "empty") ? "empty" : "trans";
			fprintf(f, "<div class='bar %s' style='left:%dpx;width:%dpx'><span class='txt'>%s</span></div>",
				cls, 80 + t.start * 5, (t.end - t.start) * 5, t.label.c_str());
		}
		fprintf(f, "</div>");
	}

	for (auto& mt : machine_tasks) {
		int m_id = mt.first;
		auto& tasks = mt.second;
		fprintf(f, "<div class='row'><span class='label-col'>机器 %d</span>", m_id);
		for (auto& t : tasks) {
			fprintf(f, "<div class='bar op' style='left:%dpx;width:%dpx'><span class='txt'>%s</span></div>",
				80 + t.start * 5, (t.end - t.start) * 5, t.label.c_str());
		}
		fprintf(f, "</div>");
	}

	fprintf(f, "</body></html>");
	fclose(f);
}
