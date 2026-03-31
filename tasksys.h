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
 *
 * FIX: completed_count is now a plain int (protected by mutex `m`)
 *      instead of atomic<int>, so it stays consistent with the
 *      mutex-guarded check inside the worker loop.
 *
 * work_available stays atomic<bool> so the spin loop can read it
 * cheaply WITHOUT acquiring the mutex on every iteration.
 * It is only WRITTEN while holding `m`, so there is no data race.
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

    std::mutex              m;
    std::condition_variable cv_done;

    IRunnable          *cur            = nullptr;
    int                 total          = 0;
    int                 next_task      = 0;
    int                 completed_count = 0;   // ← plain int, guarded by m

    std::atomic<bool>   work_available{false};
    std::atomic<bool>   stop{false};
};

/*
 * ================= Thread Pool Sleeping =================
 *
 * No atomics needed here — every shared variable is accessed
 * only while holding mutex `m`, so plain types are correct and safe.
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

    std::mutex              m;
    std::condition_variable cv_work;
    std::condition_variable cv_done;

    IRunnable *cur          = nullptr;
    int        total        = 0;
    int        next_task    = 0;
    int        completed    = 0;

    bool       work_available = false;
    bool       stop           = false;
};

#endif // _TASKSYS_H