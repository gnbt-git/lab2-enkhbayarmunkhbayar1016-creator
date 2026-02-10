#include "tasksys.h"
#include <cmath> // sin/cos ашигладаг бол хэрэгтэй

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system
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

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable *, int, const std::vector<TaskID> &) { return 0; }
void TaskSystemSerial::sync() { return; }

/*
 * ================================================================
 * Parallel + Always Spawn (Part A)
 * ================================================================
 */
const char *TaskSystemParallelSpawn::name() { return "Parallel + Always Spawn"; }

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads)
    : ITaskSystem(num_threads), nthreads(num_threads) {}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks) {
    if (num_total_tasks <= 0) return;

    int T = std::max(1, nthreads);
    int num_workers = std::min(T, num_total_tasks);

    std::atomic<int> next{0};
    std::vector<std::thread> threads;
    threads.reserve(num_workers);

    auto worker = [&]() {
        while (true) {
            int tid = next.fetch_add(1);
            if (tid >= num_total_tasks) break;
            runnable->runTask(tid, num_total_tasks);
        }
    };

    for (int i = 0; i < num_workers; i++) threads.emplace_back(worker);
    for (auto &th : threads) th.join();
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable *, int, const std::vector<TaskID> &) { return 0; }
void TaskSystemParallelSpawn::sync() { return; }

/*
 * ================================================================
 * Parallel + Thread Pool + Spin (Part A)
 * ================================================================
 */
const char *TaskSystemParallelThreadPoolSpinning::name() { return "Parallel + Thread Pool + Spin"; }

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads)
    : ITaskSystem(num_threads), nthreads(std::max(1, num_threads)) {

    workers.reserve(nthreads);
    for (int i = 0; i < nthreads; i++) {
        workers.emplace_back([this]() {
            while (!shutdown.load(std::memory_order_acquire)) {
                if (!has_work.load(std::memory_order_acquire)) {
                    // spin: жижиг pause өгч CPU-г арай бага идүүлэх боломжтой
                    std::this_thread::yield();
                    continue;
                }

                while (true) {
                    int t = next_task.fetch_add(1);
                    if (t >= cur_total) break;
                    cur_runnable->runTask(t, cur_total);
                    done_tasks.fetch_add(1);
                }

                // worker бүх task дууссан эсэхийг шалгаад ажлыг унтраана
                if (done_tasks.load() >= cur_total) {
                    has_work.store(false, std::memory_order_release);
                }
            }
        });
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    shutdown.store(true, std::memory_order_release);
    has_work.store(true, std::memory_order_release); // унтаж байвал биш spin байгаа ч exit-д тусална
    for (auto &th : workers) th.join();
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable, int num_total_tasks) {
    if (num_total_tasks <= 0) return;

    cur_runnable = runnable;
    cur_total = num_total_tasks;
    next_task.store(0);
    done_tasks.store(0);

    has_work.store(true, std::memory_order_release);

    // main thread өөрөө ч бас ажиллаж болно (optional)
    while (true) {
        int t = next_task.fetch_add(1);
        if (t >= cur_total) break;
        cur_runnable->runTask(t, cur_total);
        done_tasks.fetch_add(1);
    }

    // бүх task дуусахыг хүлээнэ
    while (done_tasks.load() < cur_total) {
        std::this_thread::yield();
    }
    has_work.store(false, std::memory_order_release);
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable *, int, const std::vector<TaskID> &) { return 0; }
void TaskSystemParallelThreadPoolSpinning::sync() { return; }

/*
 * ================================================================
 * Parallel + Thread Pool + Sleep (Part A)
 * ================================================================
 */
const char *TaskSystemParallelThreadPoolSleeping::name() { return "Parallel + Thread Pool + Sleep"; }

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads)
    : ITaskSystem(num_threads), nthreads(std::max(1, num_threads)) {

    workers.reserve(nthreads);
    for (int i = 0; i < nthreads; i++) {
        workers.emplace_back([this]() {
            while (true) {
                // ажил иртэл унтана
                std::unique_lock<std::mutex> lk(m);
                cv_work.wait(lk, [&] { return has_work || shutdown.load(); });
                if (shutdown.load()) return;

                // local copy
                IRunnable* r = cur_runnable;
                int total = cur_total;
                lk.unlock();

                // ажиллана
                while (true) {
                    int t = next_task.fetch_add(1);
                    if (t >= total) break;
                    r->runTask(t, total);
                    int finished = done_tasks.fetch_add(1) + 1;

                    // хамгийн сүүлчийн task дуусвал main-г сэрээнэ
                    if (finished == total) {
                        std::lock_guard<std::mutex> g(m);
                        has_work = false;
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
        shutdown.store(true);
        has_work = true;
    }
    cv_work.notify_all();
    for (auto &th : workers) th.join();
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable, int num_total_tasks) {
    if (num_total_tasks <= 0) return;

    {
        std::lock_guard<std::mutex> lk(m);
        cur_runnable = runnable;
        cur_total = num_total_tasks;
        next_task.store(0);
        done_tasks.store(0);
        has_work = true;
    }
    cv_work.notify_all();

    // main thread өөрөө ч бас ажиллаж болно (optional)
    while (true) {
        int t = next_task.fetch_add(1);
        if (t >= num_total_tasks) break;
        runnable->runTask(t, num_total_tasks);
        int finished = done_tasks.fetch_add(1) + 1;
        if (finished == num_total_tasks) {
            std::lock_guard<std::mutex> lk(m);
            has_work = false;
            cv_done.notify_one();
        }
    }

    // бүх task дуусахыг хүлээнэ
    std::unique_lock<std::mutex> lk(m);
    cv_done.wait(lk, [&] { return done_tasks.load() >= num_total_tasks; });
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable *, int, const std::vector<TaskID> &) { return 0; }
void TaskSystemParallelThreadPoolSleeping::sync() { return; }
