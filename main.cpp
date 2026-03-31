#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <thread>
#include <fstream>
#include <string>
#include <numeric>

#include "tasksys.h"

static std::mutex json_out_mutex;

// ── Task: маш хөнгөн (mutex overhead харагдана) ──
class LightTask : public IRunnable {
public:
    std::vector<double> results;
    LightTask(int n) : results(n, 0.0) {}

    void runTask(int taskID, int num_total) override {
        // Маш хөнгөн ажил — зөвхөн нэмэлт тооцоо
        // Ингэснээр mutex wait / busy-wait overhead тодорхой харагдана
        double v = (double)taskID * 0.001;
        results[taskID] = v * v + std::sin(v);
    }
};

// ── Task: дунд зэрэг ──
class MediumTask : public IRunnable {
public:
    std::vector<double> results;
    MediumTask(int n) : results(n, 0.0) {}

    void runTask(int taskID, int) override {
        double val = 0.0;
        for (int i = 0; i < 50; ++i)
            val += std::sin(i * 0.01 + taskID) * std::cos(i * 0.02);
        results[taskID] = val;
    }
};

// ── JSON бичих ──
void write_json(const std::string& name, double ms, int thread_count,
                double spin_ms = -1, double sleep_ms = -1) {
    std::lock_guard<std::mutex> lk(json_out_mutex);

    std::ifstream fin("results.json");
    std::string content = "[]";
    if (fin.good())
        content = std::string(std::istreambuf_iterator<char>(fin),
                              std::istreambuf_iterator<char>());
    fin.close();

    auto pos = content.rfind(']');
    if (pos != std::string::npos) content = content.substr(0, pos);
    content += (content.back() == '[') ? "\n" : ",\n";

    content += "  {\"name\": \"" + name +
               "\", \"time_ms\": " + std::to_string(ms) +
               ", \"thread_count\": " + std::to_string(thread_count);

    if (spin_ms >= 0)
        content += ", \"spin_ms\": " + std::to_string(spin_ms);
    if (sleep_ms >= 0)
        content += ", \"sleep_ms\": " + std::to_string(sleep_ms);

    content += "}\n]";

    std::ofstream fout("results.json");
    fout << content;
}

// ── Benchmark runner ──
double bench(ITaskSystem* sys, IRunnable* task, int num_tasks) {
    auto t1 = std::chrono::high_resolution_clock::now();
    sys->run(task, num_tasks);
    auto t2 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(t2 - t1).count();
}

int main() {
    // JSON цэвэрлэнэ
    std::ofstream clr("results.json"); clr << "[]"; clr.close();

    int num_threads = 4;  // яг 4 thread — ялгаа тодорхой харагдана
    int REPEAT      = 5;  // 5 удаа давтаж дундажийг авна

    // ── Хөнгөн task: 50000 × 5 = 250000 task ──
    // Spinning дээр mutex contention их → удаана
    // Sleeping дээр OS manage → хурдан
    int light_tasks = 50000;

    std::cout << "========================================\n";
    std::cout << "  Task System Benchmark\n";
    std::cout << "  Threads: " << num_threads << "\n";
    std::cout << "  Light tasks: " << light_tasks << " × " << REPEAT << " rounds\n";
    std::cout << "========================================\n\n";

    // ── 1. Serial ──
    {
        double total = 0;
        for (int r = 0; r < REPEAT; r++) {
            LightTask t(light_tasks);
            ITaskSystem* sys = new TaskSystemSerial(1);
            total += bench(sys, &t, light_tasks);
            delete sys;
        }
        double avg = total / REPEAT;
        std::cout << "[Serial]    avg " << std::fixed << std::setprecision(2)
                  << avg << " ms\n";
        write_json("Serial", avg, 1);
    }

    // ── 2. Spawn ──
    {
        double total = 0;
        for (int r = 0; r < REPEAT; r++) {
            LightTask t(light_tasks);
            ITaskSystem* sys = new TaskSystemParallelSpawn(num_threads);
            total += bench(sys, &t, light_tasks);
            delete sys;
        }
        double avg = total / REPEAT;
        std::cout << "[Spawn]     avg " << std::fixed << std::setprecision(2)
                  << avg << " ms\n";
        write_json("Spawn", avg, num_threads);
    }

    // ── 3. Spinning ──
    // Хөнгөн task + олон давталт → mutex busy-wait overhead тодорхой
    double spin_avg = 0;
    {
        ITaskSystem* sys = new TaskSystemParallelThreadPoolSpinning(num_threads);
        double total = 0;
        for (int r = 0; r < REPEAT; r++) {
            LightTask t(light_tasks);
            double ms = bench(sys, &t, light_tasks);
            total += ms;
            std::cout << "  Spin round " << r+1 << ": " << ms << " ms\n";
        }
        spin_avg = total / REPEAT;
        std::cout << "[Spinning]  avg " << std::fixed << std::setprecision(2)
                  << spin_avg << " ms\n";
        write_json("Spinning", spin_avg, num_threads);
        delete sys;
    }

    // ── 4. Sleeping ──
    // cv::wait → thread OS-д шилждэг → CPU waste байхгүй → хурдан
    double sleep_avg = 0;
    {
        ITaskSystem* sys = new TaskSystemParallelThreadPoolSleeping(num_threads);
        double total = 0;
        for (int r = 0; r < REPEAT; r++) {
            LightTask t(light_tasks);
            double ms = bench(sys, &t, light_tasks);
            total += ms;
            std::cout << "  Sleep round " << r+1 << ": " << ms << " ms\n";
        }
        sleep_avg = total / REPEAT;
        std::cout << "[Sleeping]  avg " << std::fixed << std::setprecision(2)
                  << sleep_avg << " ms\n";
        write_json("Sleeping", sleep_avg, num_threads, spin_avg, sleep_avg);
        delete sys;
    }

    // ── Summary ──
    std::cout << "\n========================================\n";
    std::cout << "  SUMMARY\n";
    std::cout << "========================================\n";
    double diff = spin_avg - sleep_avg;
    double pct  = (diff / spin_avg) * 100.0;
    std::cout << "  Spinning:  " << spin_avg  << " ms\n";
    std::cout << "  Sleeping:  " << sleep_avg << " ms\n";
    std::cout << "  Diff:      " << std::abs(diff) << " ms  ("
              << std::abs(pct) << "%)\n";
    if (diff > 0)
        std::cout << "  => Sleeping is FASTER by " << pct << "%\n";
    else
        std::cout << "  => Spinning is FASTER by " << -pct << "%\n";
    std::cout << "========================================\n";

    return 0;
}