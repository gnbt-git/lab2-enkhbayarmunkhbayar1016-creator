#include "tasksys.h"
#include <algorithm>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <fstream>
#include <chrono>
#include <string>

IRunnable::~IRunnable() {}
ITaskSystem::ITaskSystem(int) {}
ITaskSystem::~ITaskSystem() {}

// ================================================================
// Helper: Write result to JSON file (thread-safe)
// ================================================================
static std::mutex json_mutex;

void write_result_json(const std::string& name, double elapsed_ms) {
    std::lock_guard<std::mutex> lk(json_mutex);

    // Read existing results
    std::ifstream fin("results.json");
    std::string content = "[]";
    if (fin.good()) {
        content = std::string(
            std::istreambuf_iterator<char>(fin),
            std::istreambuf_iterator<char>()
        );
    }
    fin.close();

    // Remove trailing ']'
    auto pos = content.rfind(']');
    if (pos != std::string::npos) content = content.substr(0, pos);
    if (content.back() == '[') {
        content += "\n";
    } else {
        content += ",\n";
    }

    // Append new entry
    content += "  {\"name\": \"" + name + "\", \"time_ms\": " + std::to_string(elapsed_ms) + "}\n]";

    std::ofstream fout("results.json");
    fout << content;
    fout.close();
}

// ================================================================
// Serial
// ================================================================
const char *TaskSystemSerial::name() { return "Serial"; }
TaskSystemSerial::TaskSystemSerial(int n) : ITaskSystem(n) {}
TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable *runnable, int num_total_tasks) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_total_tasks; i++)
        runnable->runTask(i, num_total_tasks);
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    write_result_json("Serial", ms);
}
TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable*, int, const std::vector<TaskID>&) { return 0; }
void TaskSystemSerial::sync() {}

// ================================================================
// Parallel Spawn
// ================================================================
const char *TaskSystemParallelSpawn::name() { return "Parallel + Always Spawn"; }
TaskSystemParallelSpawn::TaskSystemParallelSpawn(int n)
    : ITaskSystem(n), nthreads(std::max(1, n)) {}
TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks) {
    int T = nthreads, N = num_total_tasks;
    if (N <= 0) return;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> th;
    th.reserve(T);
    int chunk = (N + T - 1) / T;
    for (int t = 0; t < T; t++) {
        int s = t * chunk;
        int e = std::min(N, s + chunk);
        th.emplace_back([=]() {
            for (int i = s; i < e; i++)
                runnable->runTask(i, N);
        });
    }
    for (auto &x : th) x.join();

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    write_result_json("Spawn", ms);
}
TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable*, int, const std::vector<TaskID>&) { return 0; }
void TaskSystemParallelSpawn::sync() {}

// ================================================================
// Thread Pool Spinning — RACE CONDITION FIXED WITH MUTEX
// ================================================================
const char *TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads)
    : ITaskSystem(num_threads), nthreads(std::max(1, num_threads)) {
    workers.reserve(nthreads);
    for (int t = 0; t < nthreads; t++) {
        workers.emplace_back([this]() {
            while (true) {
                int task = -1;
                IRunnable *r = nullptr;
                int N = 0;

                // ✅ FIX: Check work_available INSIDE mutex to prevent race condition
                {
                    std::lock_guard<std::mutex> lk(m);
                    if (stop.load(std::memory_order_relaxed)) return;
                    if (work_available.load(std::memory_order_relaxed) && next_task < total) {
                        r    = cur;
                        N    = total;
                        task = next_task++;
                    }
                }

                if (task == -1) {
                    // ✅ FIX: Check stop again before yielding
                    if (stop.load(std::memory_order_acquire)) return;
                    std::this_thread::yield();
                    continue;
                }

                r->runTask(task, N);

                // ✅ FIX: completed update + done check under mutex
                {
                    std::lock_guard<std::mutex> lk(m);
                    int fin = ++completed_count;
                    if (fin == N) {
                        work_available.store(false, std::memory_order_release);
                        cv_done.notify_one();
                    }
                }
            }
        });
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    {
        std::lock_guard<std::mutex> lk(m);
        stop.store(true);
    }
    // Wake all spinning threads so they can check stop flag
    for (auto &t : workers) t.join();
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable, int num_total_tasks) {
    if (num_total_tasks <= 0) return;

    auto start = std::chrono::high_resolution_clock::now();

    {
        std::lock_guard<std::mutex> lk(m);
        cur            = runnable;
        total          = num_total_tasks;
        next_task      = 0;
        completed_count = 0;
        work_available.store(true, std::memory_order_release);
    }

    // ✅ FIX: Wait on separate lock acquisition — doesn't hold lock while workers need it
    {
        std::unique_lock<std::mutex> lk(m);
        cv_done.wait(lk, [&]() {
            return completed_count == num_total_tasks;
        });
    }

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    write_result_json("Spinning", ms);
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable*, int, const std::vector<TaskID>&) { return 0; }
void TaskSystemParallelThreadPoolSpinning::sync() {}

// ================================================================
// Thread Pool Sleeping — RACE CONDITION FIXED WITH MUTEX
// ================================================================
const char *TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads)
    : ITaskSystem(num_threads), nthreads(std::max(1, num_threads)) {
    workers.reserve(nthreads);
    for (int t = 0; t < nthreads; t++) {
        workers.emplace_back([this]() {
            while (true) {
                int task = -1;
                IRunnable *r = nullptr;
                int N = 0;

                {
                    std::unique_lock<std::mutex> lk(m);
                    // ✅ FIX: Wait until there's real work OR stop signal
                    cv_work.wait(lk, [&]() {
                        return stop || (work_available && next_task < total);
                    });

                    if (stop) return;

                    r    = cur;
                    N    = total;
                    task = next_task++;

                    // ✅ FIX: If this was the last task, mark no more work
                    //    so other threads don't spin trying to grab tasks
                    if (next_task >= total) {
                        work_available = false;
                    }
                }

                r->runTask(task, N);

                // ✅ FIX: completed update under mutex, notify when all done
                {
                    std::lock_guard<std::mutex> lk(m);
                    completed++;
                    if (completed == N) {
                        cv_done.notify_one();
                    }
                }
            }
        });
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    {
        std::lock_guard<std::mutex> lk(m);
        stop = true;
    }
    cv_work.notify_all();
    for (auto &t : workers) t.join();
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable, int num_total_tasks) {
    if (num_total_tasks <= 0) return;

    auto start = std::chrono::high_resolution_clock::now();

    {
        std::lock_guard<std::mutex> lk(m);
        cur            = runnable;
        total          = num_total_tasks;
        next_task      = 0;
        completed      = 0;
        work_available = true;
    }
    // ✅ Wake ALL sleeping threads at once
    cv_work.notify_all();

    {
        std::unique_lock<std::mutex> lk(m);
        cv_done.wait(lk, [&]() { return completed == total; });
    }

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    write_result_json("Sleeping", ms);
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable*, int, const std::vector<TaskID>&) { return 0; }
void TaskSystemParallelThreadPoolSleeping::sync() {}