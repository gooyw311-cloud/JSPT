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

    jobs.clear();

    int n;
    fin >> n;

    jobs.resize(n);

    int max_machine = 0;

    /* 读取工件 */
    for (int i = 0; i < n; i++)
    {
        int op_num;
        fin >> op_num;

        jobs[i].id = i + 1;

        for (int j = 0; j < op_num; j++)
        {
            int machine, time;
            fin >> machine >> time;

            GONGXU op;

            op.id = i + 1;
            op.op_index = j + 1;
            op.jiqi_id = machine;
            op.jiagong_time = time;

            jobs[i].gongxu_set.push_back(op);

            if (machine > max_machine)
                max_machine = machine;
        }
    }

    /* 矩阵大小 = LU + machines */
    int m = max_machine + 1;

    yt.man.assign(m, vector<int>(m));
    yt.kong.assign(m, vector<int>(m));

    /* 满载矩阵 */
    for (int i = 0; i < m; i++)
        for (int j = 0; j < m; j++)
            fin >> yt.man[i][j];

    /* 空载矩阵 */
    for (int i = 0; i < m; i++)
        for (int j = 0; j < m; j++)
            fin >> yt.kong[i][j];

    yt.suanlileibei = "BU/SWV";

    fin.close();
}
void read_instances(
    const string& filename,
    vector<GONGJIAN>& jobs,
    yunshutime& yt,
    string type
)
{
    ifstream fin(filename);

    if (!fin.is_open())
    {
        cout << "无法打开算例文件: " << filename << endl;
        return;
    }

    jobs.clear();
    int n;
    fin >> n;
    jobs.resize(n);

    int max_machine = 0;

    for (int i = 0; i < n; i++)
    {
        int op_num;
        fin >> op_num;
        jobs[i].id = i + 1;

        for (int j = 0; j < op_num; j++)
        {
            int machine, time;
            fin >> machine >> time;

            GONGXU op;
            op.id = i + 1;
            op.op_index = j + 1;
            op.jiqi_id = machine;
            op.jiagong_time = time;
            jobs[i].gongxu_set.push_back(op);

            max_machine = max(max_machine, machine);
        }
    }

    // --- Loaded matrix: n rows, each with n_ops_i values ---
    yt.man_hk.resize(n);
    for (int i = 0; i < n; i++)
    {
        int n_ops = (int)jobs[i].gongxu_set.size();
        yt.man_hk[i].resize(n_ops, 0);
        for (int d = 0; d < n_ops; d++)
            fin >> yt.man_hk[i][d];

        // HK data files store n_ops+1 values per row; the extra is
        // return-to-depot time (unused in HK).  Skip it to prevent
        // parsing drift into the next row.
        {
            int dummy;
            fin >> dummy;
        }

        // Sanity check: first value should be 0 per HK definition
        if (yt.man_hk[i][0] != 0)
        {
            cerr << "[read_hk] Warning: man_hk[" << i << "][0] = "
                << yt.man_hk[i][0] << " (expected 0, forcing to 0)" << endl;
            yt.man_hk[i][0] = 0;
        }
    }

    // --- Empty matrix: m × m (machines 1..max_machine) ---
    // Pad to (m+1)×(m+1) with row/col 0 = 0  (virtual depot costs nothing)
    int m = max_machine;
    yt.kong.assign(m + 1, vector<int>(m + 1, 0));
    for (int i = 1; i <= m; i++)
        for (int j = 1; j <= m; j++)
            fin >> yt.kong[i][j];

    yt.man.clear(); // not used for HK
    yt.suanlileibei = "HK";
}
