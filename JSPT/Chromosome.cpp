#include "Chromosome.h"
#include "yunshutime.h"
#include "random.h"
#include "GONGXU.h"
#include <map>
#include <iostream>
#include <cstdio>
#include <algorithm>
#include <string>

// ============================================================
//  Constructors / destructor / copy
// ============================================================

Chromosome::Chromosome()
{
    fitness = 0;
    vehicle_assignment.clear();
    operation_sequence.clear();
    transport_sequence.clear();
    op_start.clear();
    op_end.clear();
    trans_start.clear();
    trans_end.clear();
    empty_start.clear();
    empty_end.clear();
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
    op_start = other.op_start;
    op_end = other.op_end;
    trans_start = other.trans_start;
    trans_end = other.trans_end;
    empty_start = other.empty_start;
    empty_end = other.empty_end;
}

Chromosome& Chromosome::operator=(const Chromosome& other)
{
    if (this != &other) {
        fitness = other.fitness;
        vehicle_assignment = other.vehicle_assignment;
        operation_sequence = other.operation_sequence;
        transport_sequence = other.transport_sequence;
        op_start = other.op_start;
        op_end = other.op_end;
        trans_start = other.trans_start;
        trans_end = other.trans_end;
        empty_start = other.empty_start;
        empty_end = other.empty_end;
    }
    return *this;
}

// ============================================================
//  schedule_transport
//
//  Schedules ONE transport event (index d) for job job_id.
//
//  BU/SWV mode (is_hk == false):
//    d = 0          : LU  → machine[0]
//    d = 1..n_ops-1 : machine[d-1] → machine[d]
//    d = n_ops      : machine[n_ops-1] → LU   (return trip)
//    loaded time    : yt.man[from_loc][to_loc]
//
//  HK mode (is_hk == true):
//    d = 0          : virtual depot (0) → machine[0]
//                     man_hk[j][0] == 0  →  zero cost, free positioning
//    d = 1..n_ops-1 : machine[d-1] → machine[d]
//    d = n_ops      : does NOT exist in HK (no return trip)
//    loaded time    : yt.man_hk[j][d]
//    empty  time    : yt.kong[vehicle_pos][from_loc]
//                     (kong padded with 0s at row/col 0 → depot costs nothing)
// ============================================================
void Chromosome::schedule_transport(
    int job_id,
    int d,
    const std::vector<GONGJIAN>& jobs,
    const yunshutime& yt,
    std::vector<int>& vehicle_free,
    std::vector<int>& vehicle_pos,
    std::vector<int>& trans_done,
    bool is_hk)
{
    int j = job_id - 1;
    int n_ops = (int)jobs[j].gongxu_set.size();

    if (d >= (int)vehicle_assignment[j].size()) return;
    int v_id = vehicle_assignment[j][d];
    if (v_id < 1 || v_id >= (int)vehicle_free.size()) return;

    // --- Pickup and delivery locations ---
    // from_loc : where the job currently sits (vehicle drives here empty)
    // to_loc   : where the job needs to go next
    //
    // BU/SWV: location 0 = LU (physical loading/unloading area)
    // HK    : location 0 = virtual depot (0-cost padding; no physical LU)
    int from_loc = (d == 0) ? 0 : jobs[j].gongxu_set[d - 1].jiqi_id;
    int to_loc = (d == n_ops) ? 0 : jobs[j].gongxu_set[d].jiqi_id;

    // --- Empty travel: vehicle repositions to pickup ---
    int t_empty_start = vehicle_free[v_id];
    int t_vehicle_arrive = t_empty_start + yt.kong[vehicle_pos[v_id]][from_loc];

    // --- Loaded travel: vehicle carries job to next location ---
    int loaded_time;
    if (is_hk)
    {
        // Per-(job, operation) travel time; d must be in [0, n_ops-1]
        loaded_time = (d < (int)yt.man_hk[j].size()) ? yt.man_hk[j][d] : 0;
    }
    else
    {
        // Location-to-location matrix
        loaded_time = yt.man[from_loc][to_loc];
    }

    int t_job_ready = (d == 0) ? 0 : op_end[j][d - 1];
    int t_trans_start = std::max(t_vehicle_arrive, t_job_ready);
    int t_trans_end = t_trans_start + loaded_time;

    // --- Record times ---
    empty_start[j][d] = t_empty_start;
    empty_end[j][d] = t_vehicle_arrive;
    trans_start[j][d] = t_trans_start;
    trans_end[j][d] = t_trans_end;

    // --- Advance vehicle state ---
    vehicle_free[v_id] = t_trans_end;
    vehicle_pos[v_id] = to_loc;
    trans_done[j]++;
}

// ============================================================
//  calculate
//
//  Decodes the chromosome and computes the schedule.
//
//  OS structure per job:
//    BU/SWV : job id appears (n_ops + 1) times
//             — n_ops production ops  +  1 return-to-LU transport
//    HK     : job id appears n_ops times
//             — n_ops production ops, NO return trip
//
//  Fitness:
//    BU/SWV : max over all jobs of trans_end[j][n_ops]  (back at LU)
//    HK     : max over all jobs of op_end[j][n_ops-1]   (last op done)
// ============================================================
void Chromosome::calculate(
    const yunshutime& yt,
    const std::vector<GONGJIAN>& jobs)
{
    bool is_hk = (yt.suanlileibei == "HK");
    int num_jobs = (int)jobs.size();
    if (num_jobs == 0) { fitness = 0; return; }

    // Determine number of vehicles from chromosome
    int num_vehicles = 1;
    for (const auto& va : vehicle_assignment)
        for (int v : va)
            if (v > num_vehicles) num_vehicles = v;

    std::vector<int> vehicle_free(num_vehicles + 1, 0);
    std::vector<int> vehicle_pos(num_vehicles + 1, 0);
    int num_locs = (int)yt.kong.size();          // works for both modes
    std::vector<int> machine_free(num_locs, 0);

    // --- Allocate time arrays ---
    op_end.assign(num_jobs, {});
    trans_end.assign(num_jobs, {});
    op_start.assign(num_jobs, {});
    trans_start.assign(num_jobs, {});
    empty_start.assign(num_jobs, {});
    empty_end.assign(num_jobs, {});

    for (int i = 0; i < num_jobs; ++i)
    {
        int n_ops = (int)jobs[i].gongxu_set.size();
        op_end[i].assign(n_ops, 0);
        op_start[i].assign(n_ops, 0);

        // HK: n_ops transport slots (d = 0..n_ops-1)
        // BU/SWV: n_ops+1 transport slots (d = 0..n_ops, including return)
        int n_trans = is_hk ? n_ops : n_ops + 1;
        trans_end[i].assign(n_trans, 0);
        trans_start[i].assign(n_trans, 0);
        empty_start[i].assign(n_trans, 0);
        empty_end[i].assign(n_trans, 0);
    }

    std::vector<int> trans_done(num_jobs, 0);
    std::map<int, int> op_counter;

    // --- Main decode loop ---
    for (int id : operation_sequence)
    {
        int j = id - 1;
        if (j < 0 || j >= num_jobs) continue;

        int d = op_counter[id]++;
        int n_ops = (int)jobs[j].gongxu_set.size();

        if (d < n_ops)
        {
            // ---- Production operation d ----
            // 1. Schedule the transport that brings job to this machine
            schedule_transport(id, d, jobs, yt,
                vehicle_free, vehicle_pos, trans_done, is_hk);

            // 2. Schedule the machining operation
            int machine_id = jobs[j].gongxu_set[d].jiqi_id;
            int process_time = jobs[j].gongxu_set[d].jiagong_time;
            if (machine_id < 0 || machine_id >= num_locs) continue;

            int t_start = std::max({
                trans_end[j][d],
                (d == 0 ? 0 : op_end[j][d - 1]),
                machine_free[machine_id]
                });
            int t_finish = t_start + process_time;

            op_start[j][d] = t_start;
            op_end[j][d] = t_finish;
            machine_free[machine_id] = t_finish;
        }
        else if (d == n_ops && !is_hk)
        {
            // ---- BU/SWV only: return-to-LU transport ----
            schedule_transport(id, d, jobs, yt,
                vehicle_free, vehicle_pos, trans_done, false);
        }
        // HK: d == n_ops is never reached (OS has only n_ops entries per job)
    }

    // --- Compute fitness ---
    int result = 0;
    for (int i = 0; i < num_jobs; ++i)
    {
        int n_ops = (int)jobs[i].gongxu_set.size();
        if (is_hk)
            result = std::max(result, op_end[i][n_ops - 1]);   // last op done
        else
            result = std::max(result, trans_end[i][n_ops]);    // back at LU
    }
    fitness = result;
}

// ============================================================
//  print
// ============================================================
void Chromosome::print(
    const std::vector<GONGJIAN>& jobs,
    const yunshutime& yt) const
{
    if (op_end.empty() || trans_end.empty()) {
        std::cout << "未执行调度计算！" << std::endl;
        return;
    }

    bool is_hk = (yt.suanlileibei == "HK");

    // ── 1. 工序 / 运输结束时间（原有输出）────────────────────────────────────
    for (size_t j = 0; j < op_end.size(); ++j)
        for (size_t k = 0; k < op_end[j].size(); ++k)
            printf("工件%d 工序%d 结束时间：%d\n",
                (int)(j + 1), (int)(k + 1), op_end[j][k]);

    for (size_t j = 0; j < trans_end.size(); ++j)
        for (size_t k = 0; k < trans_end[j].size(); ++k)
            printf("工件%d 运输%d 结束时间：%d\n",
                (int)(j + 1), (int)(k), trans_end[j][k]);

    // ── 2. 每辆小车的运输甘特（按时间顺序）──────────────────────────────────
    struct Event {
        int  time_start;  // 事件开始时刻
        int  time_end;    // 事件结束时刻
        int  from_loc;    // 起点（0 = LU/虚拟depot，1..m = 机床，-1 = 上一停留处）
        int  to_loc;      // 终点
        int  job_id;      // 0 = 空载，>0 = 运输该工件
        int  seg;         // 第几段运输（d，0-based）
    };

    std::map<int, std::vector<Event>> vehicle_events;

    for (int j = 0; j < (int)jobs.size(); ++j)
    {
        if (j >= (int)vehicle_assignment.size()) continue;
        int n_ops = (int)jobs[j].gongxu_set.size();
        int n_trans = is_hk ? n_ops : n_ops + 1;

        for (int d = 0; d < n_trans; ++d)
        {
            if (d >= (int)vehicle_assignment[j].size()) continue;
            int v = vehicle_assignment[j][d];

            // 起点 = 上一工序所在机床（d==0 时为 LU / 虚拟depot，即 0）
            int from_loc = (d == 0) ? 0 : jobs[j].gongxu_set[d - 1].jiqi_id;
            // 终点 = 当前工序所在机床（返回 LU 时为 0）
            int to_loc = (d == n_ops) ? 0 : jobs[j].gongxu_set[d].jiqi_id;

            // 空载段：vehicle 从上一停留处 → from_loc（pickup 点）
            // 用 from_loc 作为终点；起点标 -1 表示"由上一任务终点出发"（动态，打印时标"—"）
            int es = empty_start[j][d], ee = empty_end[j][d];
            if (ee > es)
                vehicle_events[v].push_back({ es, ee, /*from*/-1, /*to*/from_loc, /*job*/0, d });

            // 满载段：from_loc → to_loc
            int ts = trans_start[j][d], te = trans_end[j][d];
            if (te > ts)
                vehicle_events[v].push_back({ ts, te, from_loc, to_loc, j + 1, d });
        }
    }

    // 辅助：把位置编号转成可读字符串
    auto loc_name = [&](int loc) -> std::string {
        if (loc == -1) return "—";
        if (loc == 0) return is_hk ? "depot" : "LU";
        char buf[16];
        sprintf_s(buf, "M%d", loc);
        return buf;
        };

    printf("\n========== 小车运输甘特 ==========\n");

    for (auto& kv : vehicle_events)
    {
        int v = kv.first;
        auto& evs = kv.second;

        // 按开始时刻排序
        std::sort(evs.begin(), evs.end(),
            [](const Event& a, const Event& b) { return a.time_start < b.time_start; });

        printf("\n【车辆 %d】\n", v);
        printf("  %-6s  %-7s  %-7s  %-6s  %-6s  说明\n",
            "类型", "起点", "终点", "开始", "结束");
        printf("  %s\n", std::string(52, '-').c_str());

        for (const auto& e : evs)
        {
            if (e.job_id == 0)
            {
                printf("  %-6s  %-7s  %-7s  %-6d  %-6d  空载移动\n",
                    "空载",
                    loc_name(e.from_loc).c_str(),
                    loc_name(e.to_loc).c_str(),
                    e.time_start, e.time_end);
            }
            else
            {
                printf("  %-6s  %-7s  %-7s  %-6d  %-6d  运输 工件%d 第%d段\n",
                    "运输",
                    loc_name(e.from_loc).c_str(),
                    loc_name(e.to_loc).c_str(),
                    e.time_start, e.time_end,
                    e.job_id, e.seg + 1);
            }
        }
    }

    printf("\n===================================\n");
}

// ============================================================
//  generate_gantt
// ============================================================
void Chromosome::generate_gantt(
    const std::vector<GONGJIAN>& jobs,
    const yunshutime& yt,
    const char* filename) const
{
    bool is_hk = (yt.suanlileibei == "HK");

    FILE* f = nullptr;
    if (fopen_s(&f, filename, "wb") != 0 || !f) {
        std::cerr << "甘特图文件创建失败！" << std::endl;
        return;
    }
    unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    fwrite(bom, 1, 3, f);

    struct Task { int start, end, job; std::string label, type; };
    std::map<int, std::vector<Task>> vehicle_tasks;
    std::map<int, std::vector<Task>> machine_tasks;

    for (int j = 0; j < (int)jobs.size(); j++)
    {
        if (j >= (int)vehicle_assignment.size()) continue;
        int n_ops = (int)jobs[j].gongxu_set.size();
        // Number of transport slots: n_ops for HK, n_ops+1 for BU/SWV
        int n_trans = is_hk ? n_ops : n_ops + 1;

        for (int k = 0; k < n_trans; k++)
        {
            if (k >= (int)vehicle_assignment[j].size()) continue;
            int v_id = vehicle_assignment[j][k];
            if (v_id < 1) continue;

            int from_loc = (k == 0) ? 0 : jobs[j].gongxu_set[k - 1].jiqi_id;
            int to_loc = (k == n_ops) ? 0 : jobs[j].gongxu_set[k].jiqi_id;

            // Empty leg
            int es = empty_start[j][k], ee = empty_end[j][k];
            if (ee > es) {
                char buf[128];
                if (from_loc == 0 && !is_hk)
                    sprintf_s(buf, "空载→仓库");
                else if (from_loc == 0)
                    sprintf_s(buf, "空载→机床%d", to_loc);
                else
                    sprintf_s(buf, "空载→机床%d", from_loc);
                vehicle_tasks[v_id].push_back({ es, ee, 0, buf, "empty" });
            }

            // Loaded leg
            int ts = trans_start[j][k], te = trans_end[j][k];
            if (te > ts) {
                char buf[128];
                if (!is_hk) {
                    if (from_loc == 0)
                        sprintf_s(buf, "J%d: 仓库→机床%d", j + 1, to_loc);
                    else if (to_loc == 0)
                        sprintf_s(buf, "J%d: 机床%d→仓库", j + 1, from_loc);
                    else
                        sprintf_s(buf, "J%d: 机床%d→机床%d", j + 1, from_loc, to_loc);
                }
                else {
                    sprintf_s(buf, "J%d: 机床%d→机床%d", j + 1, from_loc, to_loc);
                }
                vehicle_tasks[v_id].push_back({ ts, te, j + 1, buf, "load" });
            }

            // Machine operation (only for production ops, k < n_ops)
            if (k < n_ops && k < (int)op_end[j].size()) {
                int m_id = jobs[j].gongxu_set[k].jiqi_id;
                char buf[64];
                sprintf_s(buf, "J%d-O%d", j + 1, k + 1);
                machine_tasks[m_id].push_back(
                    { op_start[j][k], op_end[j][k], j + 1, buf, "op" });
            }
        }
    }

    fprintf(f, "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<title>甘特图</title><style>");
    fprintf(f, "body{font-family:Microsoft YaHei,Arial;margin:20px}"
        ".row{height:40px;position:relative;border-bottom:1px solid #ddd}"
        ".label-col{width:100px;display:inline-block;font-weight:bold;line-height:40px}"
        ".bar{height:20px;position:absolute;top:10px}"
        ".op{background:#4CAF50}.trans{background:#2196F3}.empty{background:#FFA726;opacity:0.7}"
        ".txt{position:absolute;top:-18px;left:2px;color:#000;font-size:10px;"
        "white-space:nowrap;font-weight:bold}");
    fprintf(f, "</style></head><body><h2>调度甘特图 (ET: %d)</h2>", fitness);

    for (auto& vt : vehicle_tasks) {
        fprintf(f, "<div class='row'><span class='label-col'>车辆 %d</span>", vt.first);
        for (auto& t : vt.second) {
            const char* cls = (t.type == "empty") ? "empty" : "trans";
            fprintf(f, "<div class='bar %s' style='left:%dpx;width:%dpx'>"
                "<span class='txt'>%s</span></div>",
                cls, 100 + t.start * 5, (t.end - t.start) * 5, t.label.c_str());
        }
        fprintf(f, "</div>");
    }

    for (auto& mt : machine_tasks) {
        fprintf(f, "<div class='row'><span class='label-col'>机床 %d</span>", mt.first);
        for (auto& t : mt.second) {
            fprintf(f, "<div class='bar op' style='left:%dpx;width:%dpx'>"
                "<span class='txt'>%s</span></div>",
                100 + t.start * 5, (t.end - t.start) * 5, t.label.c_str());
        }
        fprintf(f, "</div>");
    }

    fprintf(f, "</body></html>");
    fclose(f);
}