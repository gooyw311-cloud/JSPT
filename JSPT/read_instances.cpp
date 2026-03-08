#include "read_instances.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>
#include "GONGXU.h"
#include "GONGJIAN.h"
#include "yunshutime.h"

using namespace std;


/* =========================
   根据文件名识别算例类型
   ========================= */
string detect_instance_type(const string& filename)
{
    if (filename.find("EX") != string::npos)
        return "BU";

    if (filename.find("P1") != string::npos ||
        filename.find("P2") != string::npos)
        return "HK";

    return "SWV";
}


/* =========================
   BU 工件读取
   =========================
   文件格式：

   n
   op_num m t m t ...
   op_num m t m t ...
*/

void read_BU_jobs(ifstream& fin, vector<GONGJIAN>& jobs)
{
    jobs.clear();

    int n;
    fin >> n;

    jobs.resize(n);

    for (int i = 0; i < n; i++)
    {
        int op_num;
        fin >> op_num;

        jobs[i].id = i + 1;     // 工件编号从1开始

        for (int j = 0; j < op_num; j++)
        {
            int machine, time;
            fin >> machine >> time;

            GONGXU op;

            op.id = i + 1;          // 工件编号
            op.op_index = j + 1;    // 工序编号
            op.jiqi_id = machine;   // 机器编号从1开始
            op.jiagong_time = time;

            jobs[i].gongxu_set.push_back(op);
        }
    }
}


/* =========================
   BU 运输矩阵读取
   ========================= */

void read_BU_transport(ifstream& fin, yunshutime& yt)
{
    int m = 4;

    yt.BU.assign(m, vector<int>(m));

    for (int i = 0; i < m; i++)
        for (int j = 0; j < m; j++)
            fin >> yt.BU[i][j];

    yt.suanlileibei = "BU";
}


/* =========================
   HK / SWV 工件读取
   =========================
   文件格式：

   n m
   machine time machine time ...
*/

void read_HK_jobs(ifstream& fin, vector<GONGJIAN>& jobs, int& m)
{
    int n;

    fin >> n >> m;

    jobs.resize(n);

    for (int i = 0; i < n; i++)
    {
        jobs[i].id = i + 1;

        for (int j = 0; j < m; j++)
        {
            int machine, time;
            fin >> machine >> time;

            GONGXU op;

            op.id = i + 1;
            op.op_index = j + 1;
            op.jiqi_id = machine;   // 机器编号从1开始
            op.jiagong_time = time;

            jobs[i].gongxu_set.push_back(op);
        }
    }
}


/* =========================
   HK 运输矩阵
   ========================= */

void read_HK_transport(ifstream& fin, yunshutime& yt, int m)
{
    yt.man.assign(m, vector<int>(m));
    yt.kong.assign(m, vector<int>(m));

    // 满载运输时间
    for (int i = 0; i < m; i++)
        for (int j = 0; j < m; j++)
            fin >> yt.man[i][j];

    // 空载运输时间
    for (int i = 0; i < m; i++)
        for (int j = 0; j < m; j++)
            fin >> yt.kong[i][j];

    yt.suanlileibei = "HK";
}


/* =========================
   SWV 工件读取
   ========================= */

void read_SWV_jobs(ifstream& fin, vector<GONGJIAN>& jobs, int& m)
{
    read_HK_jobs(fin, jobs, m);
}


/* =========================
   统一算例读取入口
   ========================= */

void read_instances(
    const string& filename,
    vector<GONGJIAN>& jobs,
    yunshutime& yt
)
{
    ifstream fin(filename);

    if (!fin.is_open())
    {
        cout << "无法打开算例文件: " << filename << endl;
        return;
    }

    string type = detect_instance_type(filename);

    if (type == "BU")
    {
        read_BU_jobs(fin, jobs);
        read_BU_transport(fin, yt);
    }
    else if (type == "HK")
    {
        int m;
        read_HK_jobs(fin, jobs, m);
        read_HK_transport(fin, yt, m);
    }
    else
    {
        int m;
        read_SWV_jobs(fin, jobs, m);
        read_HK_transport(fin, yt, m);
    }

    fin.close();
}