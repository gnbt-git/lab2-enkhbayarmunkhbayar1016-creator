#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <functional>
#include <cstdlib>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;
using namespace chrono;

static const int WARMUP = 3;
static const int RUNS = 10;
static const int MAX_THREADS = 16;   /

static double mean_ms(const vector<double>& v) {
    if (v.empty()) return 0.0;
    return accumulate(v.begin(), v.end(), 0.0) / (double)v.size();
}

static double p95_ms(vector<double> v) {
    if (v.empty()) return 0.0;
    sort(v.begin(), v.end());
    int n = (int)v.size();
    int idx = (int)ceil(0.95 * n) - 1;   
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    return v[idx];
}

static double measure_ms(const function<void()>& f) {
    auto s = steady_clock::now();
    f();
    auto e = steady_clock::now();
    return duration<double, milli>(e - s).count();
}

static void transform_serial(vector<double>& A) {
    for (size_t i = 0; i < A.size(); i++)
        A[i] = sin(A[i]) * 0.5 + 0.25;
}


static void worker_transform(int s, int e, vector<double>& A) {
    for (int i = s; i < e; i++)
        A[i] = sin(A[i]) * 0.5 + 0.25;
}

static void transform_thread(vector<double>& A, int T) {
    int N = (int)A.size();
    if (T < 1) T = 1;
    if (T > N) T = N;

    vector<thread> th;
    th.reserve(T);

    int step = N / T;
    int start = 0;

    for (int t = 0; t < T; t++) {
        int end = (t == T - 1) ? N : (start + step);
        th.emplace_back(worker_transform, start, end, ref(A));
        start = end;
    }

    for (auto& x : th) x.join();
}


static void transform_omp(vector<double>& A, int T) {
#ifdef _OPENMP
    if (T < 1) T = 1;
    omp_set_num_threads(T);

#pragma omp parallel for
    for (int i = 0; i < (int)A.size(); i++)
        A[i] = sin(A[i]) * 0.5 + 0.25;
#else
    (void)T;
    transform_serial(A);
#endif
}

int main(int argc, char** argv) {
    long long SIZE = 80000000LL; 

    
    if (argc >= 2) {
        long long x = atoll(argv[1]);
        if (x > 0) SIZE = x;
    }

    cout << "workload,impl,threads,size,run_i,elapsed_ms,notes\n";

    auto run_test = [&](const string& impl, int T,
                        const function<void(vector<double>&)>& func) {
        vector<double> times;
        times.reserve(RUNS);

        // Warm-up 3
        for (int w = 0; w < WARMUP; w++) {
            vector<double> A((size_t)SIZE, 1.0);
            func(A);
        }

        // Run 10
        for (int r = 0; r < RUNS; r++) {
            vector<double> A((size_t)SIZE, 1.0);

            double t = measure_ms([&]() { func(A); });
            times.push_back(t);

            cout << "transform," << impl << ","
                 << T << "," << SIZE << ","
                 << r << "," << t
                 << ",res=" << A[0] << "\n";
        }

        cout << "transform," << impl << ","
             << T << "," << SIZE
             << ",mean," << mean_ms(times) << ",\n";

        cout << "transform," << impl << ","
             << T << "," << SIZE
             << ",p95," << p95_ms(times) << ",\n";
    };


    run_test("serial", 1, [](vector<double>& A) { transform_serial(A); });

    for (int T = 1; T <= MAX_THREADS; T *= 2) {
        run_test("threads", T, [&](vector<double>& A) { transform_thread(A, T); });
        run_test("openmp",  T, [&](vector<double>& A) { transform_omp(A, T); });
    }

    return 0;
}