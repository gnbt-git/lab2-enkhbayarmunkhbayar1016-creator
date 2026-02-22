// itasksys.h
#ifndef _ITASKSYS_H
#define _ITASKSYS_H

#include <vector>

typedef int TaskID;

class IRunnable {
public:
    virtual ~IRunnable();

    // task_id: 0..num_total_tasks-1
    // num_total_tasks: total number of tasks
    virtual void runTask(int task_id, int num_total_tasks) = 0;
};

class ITaskSystem {
public:
    ITaskSystem(int num_threads);
    virtual ~ITaskSystem();
    virtual const char *name() = 0;

    // Synchronous bulk task launch
    virtual void run(IRunnable *runnable, int num_total_tasks) = 0;

    // Async with deps (not required for Part A)
    virtual TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                    const std::vector<TaskID> &deps) = 0;

    // Wait for all prior tasks
    virtual void sync() = 0;
};

#endif