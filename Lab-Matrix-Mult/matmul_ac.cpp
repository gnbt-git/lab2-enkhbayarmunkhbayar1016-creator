#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;
using namespace chrono;

static void init_matrix(vector<vector<float>>& M, int N) {
    mt19937 gen(42);
    uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            M[i][j] = dist(gen);
}

static void transpose(const vector<vector<float>>& B,
                      vector<vector<float>>& Bt, int N) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            Bt[j][i] = B[i][j];
}


static void matmul_seq(const vector<vector<float>>& A,
                       const vector<vector<float>>& Bt,
                       vector<vector<float>>& C, int N) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;      // register
            for (int k = 0; k < N; k++)
                sum += A[i][k] * Bt[j][k];
            C[i][j] = sum;
        }
}

// Thread worker
static void worker(int s, int e, int N,
                   const vector<vector<float>>& A,
                   const vector<vector<float>>& Bt,
                   vector<vector<float>>& C) {
    for (int i = s; i < e; i++)
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int k = 0; k < N; k++)
                sum += A[i][k] * Bt[j][k];
            C[i][j] = sum;
        }
}

// std::thread (T threads)
static void matmul_thread(const vector<vector<float>>& A,
                          const vector<vector<float>>& Bt,
                          vector<vector<float>>& C,
                          int N, int T) {
    vector<thread> th;
    th.reserve(T);

    int step = N / T;
    for (int t = 0; t < T; t++) {
        int s = t * step;
        int e = (t == T - 1) ? N : (s + step);
        th.emplace_back(worker, s, e, N, cref(A), cref(Bt), ref(C));
    }
    for (auto& x : th) x.join();
}

// OpenMP (T threads)
static void matmul_omp(const vector<vector<float>>& A,
                       const vector<vector<float>>& Bt,
                       vector<vector<float>>& C, int N, int T) {
#ifdef _OPENMP
    omp_set_num_threads(T);
#pragma omp parallel for
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int k = 0; k < N; k++)
                sum += A[i][k] * Bt[j][k];
            C[i][j] = sum;
        }
#else
    (void)T;
    matmul_seq(A, Bt, C, N);
#endif
}

template<typename F>
static double measure(F f) {
    auto s = high_resolution_clock::now();
    f();
    auto e = high_resolution_clock::now();
    return duration<double>(e - s).count();
}

int main() {
    const int N = 512;
    const int MAX_T = 15;

    cout << "==============================\n";
    cout << "MODE: RUN THIS ON BATTERY OR AC (YOU CHOOSE)\n";
    cout << "==============================\n";

    vector<vector<float>> A(N, vector<float>(N));
    vector<vector<float>> B(N, vector<float>(N));
    vector<vector<float>> Bt(N, vector<float>(N));

    init_matrix(A, N);
    init_matrix(B, N);
    transpose(B, Bt, N);

    // 1) Sequential baseline (1 удаа)
    vector<vector<float>> Cseq(N, vector<float>(N));
    double t_seq = measure([&]() { matmul_seq(A, Bt, Cseq, N); });

    cout << "N=" << N << "\n";
    cout << "t_seq=" << t_seq << "\n";
    cout << "threads,t_thread,t_openmp\n";

    // 2) T = 1..15 benchmark
    for (int T = 1; T <= MAX_T; T++) {
        vector<vector<float>> Cthr(N, vector<float>(N));
        vector<vector<float>> Comp(N, vector<float>(N));

        double t_thr = measure([&]() { matmul_thread(A, Bt, Cthr, N, T); });
        double t_omp = measure([&]() { matmul_omp(A, Bt, Comp, N, T); });

        cout << T << "," << t_thr << "," << t_omp << "\n";
    }

    return 0;
}
