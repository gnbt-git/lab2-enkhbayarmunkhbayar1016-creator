// tasksys.h
#ifndef _TASKSYS_H
#define _TASKSYS_H

#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "itasksys.h"

/*
 * ================= Serial =================
 */
class TaskSystemSerial : public ITaskSystem {
public:
    TaskSystemSerial(int num_threads);
    ~TaskSystemSerial();
    const char *name();
    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();
};

/*
 * ================= Parallel Spawn =================
 */
class TaskSystemParallelSpawn : public ITaskSystem {
public:
    TaskSystemParallelSpawn(int num_threads);
    ~TaskSystemParallelSpawn();
    const char *name();
    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();

private:
    int nthreads;
};

/*
 * ================= Thread Pool Spinning =================
 */
class TaskSystemParallelThreadPoolSpinning : public ITaskSystem {
public:
    TaskSystemParallelThreadPoolSpinning(int num_threads);
    ~TaskSystemParallelThreadPoolSpinning();
    const char *name();
    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();

private:
    int nthreads;
    std::vector<std::thread> workers;

    std::mutex m;
    IRunnable *cur = nullptr;
    int total = 0;
    int next_task = 0;

    std::atomic<int> completed{0};
    std::atomic<bool> work_available{false};
    std::atomic<bool> stop{false};

    std::condition_variable cv_done;
};

/*
 * ================= Thread Pool Sleeping =================
 */
class TaskSystemParallelThreadPoolSleeping : public ITaskSystem {
public:
    TaskSystemParallelThreadPoolSleeping(int num_threads);
    ~TaskSystemParallelThreadPoolSleeping();
    const char *name();
    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();

private:
    int nthreads;
    std::vector<std::thread> workers;

    std::mutex m;
    std::condition_variable cv_work;
    std::condition_variable cv_done;

    IRunnable *cur = nullptr;
    int total = 0;
    int next_task = 0;
    int completed = 0;

    bool work_available = false;
    bool stop = false;
};

#endif // _TASKSYS_H