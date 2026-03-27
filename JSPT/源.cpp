#include <iostream>
#include <vector>
#include "GONGXU.h"
#include "GONGJIAN.h"
#include "yunshutime.h"
#include "read_instances.h"
#include "random.h"
#include <map>

using namespace std;


void print_instances(const vector<GONGJIAN>& jobs, const yunshutime& yt)
{
    cout << "========== Instance Verification ==========" << endl;
    cout << "Instance type   : " << yt.suanlileibei << endl;
    cout << "Number of jobs  : " << jobs.size() << endl;
    cout << endl;

    /* 打印工件 */
    for (const auto& job : jobs)
    {
        cout << "Job " << job.id << " (" << job.gongxu_set.size() << " operations):" << endl;

        for (const auto& op : job.gongxu_set)
        {
            cout << "   Op " << op.op_index
                 << " -> machine " << op.jiqi_id
                 << ", process time " << op.jiagong_time << endl;
        }

        cout << endl;
    }

    /* 打印运输矩阵 */
    if (!yt.man.empty() && !yt.kong.empty())
    {
        int m = yt.man.size();

        cout << "Loaded transport matrix (man) (" << m << "x" << m << "):" << endl;

        for (int i = 0; i < m; i++)
        {
            for (int j = 0; j < m; j++)
                cout << yt.man[i][j] << " ";
            cout << endl;
        }

        cout << endl;

        cout << "Empty transport matrix (kong) (" << m << "x" << m << "):" << endl;

        for (int i = 0; i < m; i++)
        {
            for (int j = 0; j < m; j++)
                cout << yt.kong[i][j] << " ";
            cout << endl;
        }
    }
    else
    {
        cout << "Transport matrix is empty." << endl;
    }

    cout << "============================================" << endl;
}

void print_chromosome(const Chromosome& chro, const std::vector<GONGJIAN>& jobs) {

    std::cout << "Layer 1 (Vehicle Assignment):" << std::endl;
    for (size_t i = 0; i < chro.vehicle_assignment.size(); ++i) {
        std::cout << "  Job " << (i + 1) << " Transports: ";
        for (int v : chro.vehicle_assignment[i]) std::cout << "V" << v << " ";
        std::cout << std::endl;
    }

    // 辅助计数器，记录每个工件在序列中是第几次出现
    std::map<int, int> os_counter;
    std::map<int, int> ts_counter;

    // 验证第二层 (OS) -> 对应 4 台机器
    std::cout << "Layer 2 (OS) machine:" << std::endl;
    std::cout << "OS sequence: [ ";
    for (int id : chro.operation_sequence) std::cout << id << " ";
    std::cout << "]" << std::endl;
    
    std::cout << "logic: ";
    for (int id : chro.operation_sequence) {
        int occurrence = os_counter[id]++; // 第几次出现 [cite: 3]
        // 查找对应的工序和机器 ID
        int m_id = jobs[id - 1].gongxu_set[occurrence].jiqi_id;
        std::cout << "J" << id << "O" << (occurrence + 1) << "(M" << m_id << ") -> ";
    }
    std::cout << "END" << std::endl;

    // 验证第三层 (TS) -> 对应 2 辆车
    std::cout << "\nLayer 3 (TS) vehicle:" << std::endl;
    std::cout << ": [ ";
    for (int id : chro.transport_sequence) std::cout << id << " ";
    std::cout << "]" << std::endl;

    std::cout << "logic: ";
    for (int id : chro.transport_sequence) {
        int occurrence = ts_counter[id]++; // 第几次出现 
        // 从第一层获取分配给该次运输的车辆 ID
        int v_id = chro.vehicle_assignment[id - 1][occurrence];
        std::cout << "J" << id << "T" << (occurrence + 1) << "(V" << v_id << ") -> ";
    }
    std::cout << "END" << std::endl;
    std::cout << "====================================================" << std::endl;
}
int main()
{
    vector<GONGJIAN> jobs1,jobs2,jobs3;
    yunshutime yt1,yt2,yt3;

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

    
    read_instances(
        "SWV1.txt",
        jobs3,
        yt3
    );
    print_instances(jobs3, yt3);

    // 假设有 2 辆 AGV 车辆
    int num_agvsBU = 2;
    int num_agvsHK = 1;
    int num_agvsSWV = 5;
    
   // 调用函数生成初始染色体
    Chromosome test_chro = generate_random_chromosome(jobs1, num_agvsBU);
    print_chromosome(test_chro, jobs1);
	test_chro.calculate(yt1, jobs1); // 计算适应度
	test_chro.print(); // 打印染色体信息
	test_chro.generate_gantt(jobs1,yt1, "gantt_chart.txt"); // 生成甘特图
	cout << "tiaoshi" << endl;
    Chromosome test_chro2 = generate_random_chromosome(jobs2, num_agvsHK);
    print_chromosome(test_chro2, jobs2);
	test_chro2.calculate(yt2, jobs2); // 计算适应度
	test_chro2.print(); // 打印染色体信息
    Chromosome test_chro3 = generate_random_chromosome(jobs3, num_agvsSWV);
    print_chromosome(test_chro3, jobs3);
	test_chro3.calculate(yt3, jobs3); // 计算适应度
	test_chro3.print(); // 打印染色体信息

    return 0;
}