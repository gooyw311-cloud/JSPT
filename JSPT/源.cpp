#include <iostream>
#include <vector>
#include "GONGXU.h"
#include "GONGJIAN.h"
#include "yunshutime.h"
#include "read_instances.h"
using namespace std;

#include <iostream>
#include <vector>
#include <string>
#include "GONGJIAN.h"
#include "yunshutime.h"

using namespace std;

void print_instances(const vector<GONGJIAN>& jobs, const yunshutime& yt) {
    cout << "========== Instance Verification ==========" << endl;
    cout << "Instance type   : " << yt.suanlileibei << endl;
    cout << "Number of jobs  : " << jobs.size() << endl;
    cout << endl;

    // 打印每个工件的工序
    for (const auto& job : jobs) {
        cout << "Job " << job.id << " (" << job.gongxu_set.size() << " operations):" << endl;
        for (const auto& op : job.gongxu_set) {
            cout << "   Op " << op.op_index
                 << " -> machine " << op.jiqi_id
                 << ", process time " << op.jiagong_time << endl;
        }
        cout << endl;
    }

    // 打印运输时间矩阵（根据算例类型）
    if (yt.suanlileibei == "BU") {
        if (!yt.BU.empty()) {
            int m = (int)yt.BU.size();
            cout << "BU transport matrix (" << m << "x" << m << "):" << endl;
            for (int i = 0; i < m; ++i) {
                for (int j = 0; j < m; ++j) {
                    cout << yt.BU[i][j] << " ";
                }
                cout << endl;
            }
        } else {
            cout << "BU transport matrix is empty." << endl;
        }
    }
    else if (yt.suanlileibei == "HK" || yt.suanlileibei == "SWV") {
        if (!yt.man.empty() && !yt.kong.empty()) {
            int m = (int)yt.man.size();
            cout << "Loaded transport matrix (man)  (" << m << "x" << m << "):" << endl;
            for (int i = 0; i < m; ++i) {
                for (int j = 0; j < m; ++j) {
                    cout << yt.man[i][j] << " ";
                }
                cout << endl;
            }
            cout << "Empty transport matrix (kong) (" << m << "x" << m << "):" << endl;
            for (int i = 0; i < m; ++i) {
                for (int j = 0; j < m; ++j) {
                    cout << yt.kong[i][j] << " ";
                }
                cout << endl;
            }
        } else {
            cout << "Transport matrices are empty." << endl;
        }
    }
    else {
        cout << "Unknown instance type." << endl;
    }
    cout << "============================================" << endl;
}

int main()
{
    vector<GONGJIAN> jobs1,jobs2;
    yunshutime yt1,yt2;

    read_instances(
        "EX101.txt",
        jobs1,
        yt1
    );
    print_instances(jobs1, yt1);
    
    read_instances(
        "HK1.txt",
        jobs2,
        yt2
    );
    print_instances(jobs2, yt2);

    return 0;
}