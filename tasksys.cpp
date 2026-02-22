// tasksys.cpp  (FULL FIXED)
#include "tasksys.h"

#include <algorithm>   // std::min, std::max
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int /*num_threads*/) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char *TaskSystemSerial::name() { return "Serial"; }

TaskSystemSerial::TaskSystemSerial(int num_threads) : ITaskSystem(num_threads) {}
TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable *runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable * /*runnable*/, int /*num_total_tasks*/,
                                          const std::vector<TaskID> & /*deps*/) {
    // Not required for this lab
    return 0;
}

void TaskSystemSerial::sync() {
    // Not required for this lab
}

/*
 * ================================================================
 * Parallel Spawn task system implementation
 * ================================================================
 */

const char *TaskSystemParallelSpawn::name() { return "Parallel + Always Spawn"; }

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads)
    : ITaskSystem(num_threads), nthreads(std::max(1, num_threads)) {}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks) {
    int T = std::max(1, nthreads);
    int N = num_total_tasks;
    if (N <= 0) return;

    std::vector<std::thread> th;
    th.reserve(T);

    int chunk = (N + T - 1) / T; // static scheduling

    for (int t = 0; t < T; t++) {
        int start = t * chunk;
        int end = std::min(N, start + chunk);

        th.emplace_back([=]() {
            for (int i = start; i < end; i++) {
                runnable->runTask(i, N);
            }
        });
    }

    for (auto &x : th) x.join();
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable * /*runnable*/, int /*num_total_tasks*/,
                                                 const std::vector<TaskID> & /*deps*/) {
    // Not required for this lab
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // Not required for this lab
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning task system implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads)
    : ITaskSystem(num_threads), nthreads(std::max(1, num_threads)) {

    workers.reserve(nthreads);

    for (int t = 0; t < nthreads; t++) {
        workers.emplace_back([this]() {
            while (!stop.load(std::memory_order_relaxed)) {

                // Busy wait for work
                if (!work_available.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                    continue;
                }

                int task = -1;
                IRunnable *r = nullptr;
                int N = 0;

                {
                    std::lock_guard<std::mutex> lk(m);
                    r = cur;
                    N = total;
                    if (next_task < total) {
                        task = next_task++;
                    }
                }

                if (task == -1) {
                    // no task left (but others might still run)
                    std::this_thread::yield();
                    continue;
                }

                r->runTask(task, N);

                int fin = completed.fetch_add(1) + 1;
                if (fin == N) {
                    // last task finishes: signal master
                    work_available.store(false, std::memory_order_release);
                    cv_done.notify_one();
                }
            }
        });
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    stop.store(true);
    work_available.store(true); // let workers exit spin loop
    for (auto &t : workers) t.join();
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable, int num_total_tasks) {
    if (num_total_tasks <= 0) return;

    {
        std::lock_guard<std::mutex> lk(m);
        cur = runnable;
        total = num_total_tasks;
        next_task = 0;
        completed.store(0);
    }

    work_available.store(true, std::memory_order_release);

    // Master waits for completion
    std::unique_lock<std::mutex> lk(m);
    cv_done.wait(lk, [&]() { return completed.load() == num_total_tasks; });
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable * /*runnable*/, int /*num_total_tasks*/,
                                                              const std::vector<TaskID> & /*deps*/) {
    // Not required for this lab
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // Not required for this lab
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping task system implementation
 * ================================================================
 */

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

                    // Sleep until there is work or stop
                    cv_work.wait(lk, [&]() { return stop || work_available; });
                    if (stop) return;

                    r = cur;
                    N = total;

                    if (next_task < total) {
                        task = next_task++;
                    } else {
                        // no work right now, loop back to wait
                        continue;
                    }
                }

                // execute outside lock
                r->runTask(task, N);

                {
                    std::lock_guard<std::mutex> lk(m);
                    completed++;
                    if (completed == total) {
                        work_available = false;
                        cv_done.notify_one(); // wake master
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
        work_available = true; // wake workers so they can exit
    }
    cv_work.notify_all();
    for (auto &t : workers) t.join();
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable, int num_total_tasks) {
    if (num_total_tasks <= 0) return;

    {
        std::lock_guard<std::mutex> lk(m);
        cur = runnable;
        total = num_total_tasks;
        next_task = 0;
        completed = 0;
        work_available = true;
    }

    // Wake up all workers
    cv_work.notify_all();

    // Master waits for completion
    std::unique_lock<std::mutex> lk(m);
    cv_done.wait(lk, [&]() { return completed == total; });
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable * /*runnable*/, int /*num_total_tasks*/,
                                                              const std::vector<TaskID> & /*deps*/) {
    // Part B (not required here)
    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {
    // Part B (not required here)
}