// main.cpp (Benchmark test)  -- OPTIONAL, but complete
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <thread>

#include "tasksys.h"

// Compute workload
class ComputeTask : public IRunnable {
public:
    std::vector<double> results;
    int workload_intensity;

    ComputeTask(int num_tasks, int intensity)
        : results(num_tasks, 0.0), workload_intensity(intensity) {}

    void runTask(int taskID, int /*num_total_tasks*/) override {
        double val = 0.0;
        for (int i = 0; i < workload_intensity; ++i) {
            val += std::sin(i * 0.01 + taskID) * std::cos(i * 0.02 + taskID);
        }
        results[taskID] = val;
    }
};

static void runBenchmark(ITaskSystem *system, IRunnable *task,
                         int num_tasks, const std::string &name) {
    std::cout << "Testing [" << name << "]..." << std::flush;
    auto start = std::chrono::high_resolution_clock::now();

    system->run(task, num_tasks);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << " Done. Time: " << std::fixed << std::setprecision(4)
              << elapsed.count() << "s\n";
}

int main() {
    int num_threads = (int)std::thread::hardware_concurrency();
    if (num_threads <= 0) num_threads = 4;

    int num_tasks = 5000;
    int workload_intensity = 200;

    std::cout << "========================================\n";
    std::cout << "Task System Benchmark\n";
    std::cout << "Threads: " << num_threads << ", Tasks: " << num_tasks << "\n";
    std::cout << "========================================\n";

    ComputeTask task(num_tasks, workload_intensity);

    ITaskSystem *serialSystem = new TaskSystemSerial(num_threads);
    runBenchmark(serialSystem, &task, num_tasks, "Serial System");
    delete serialSystem;

    ITaskSystem *spawnSystem = new TaskSystemParallelSpawn(num_threads);
    runBenchmark(spawnSystem, &task, num_tasks, "Parallel Spawn");
    delete spawnSystem;

    ITaskSystem *spinSystem = new TaskSystemParallelThreadPoolSpinning(num_threads);
    runBenchmark(spinSystem, &task, num_tasks, "Parallel Spinning Pool");
    delete spinSystem;

    ITaskSystem *sleepSystem = new TaskSystemParallelThreadPoolSleeping(num_threads);
    runBenchmark(sleepSystem, &task, num_tasks, "Parallel Sleeping Pool");
    delete sleepSystem;

    return 0;
}