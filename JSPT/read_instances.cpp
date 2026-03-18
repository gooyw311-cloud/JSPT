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

    yt.suanlileibei = "Unified";

    fin.close();
}
