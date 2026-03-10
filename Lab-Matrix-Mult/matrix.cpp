#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip>
#include <fstream>
#include <string>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;
using namespace chrono;

// Initialize matrix
void init_matrix(vector<vector<float>>& M, int N) {
    mt19937 gen(42);
    uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            M[i][j] = dist(gen);
}

// B -> Bt (transpose + flatten)
void transpose_flat(const vector<vector<float>>& B, vector<float>& Bt, int N) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            Bt[j * N + i] = B[i][j];
}

// Sequential multiplication A × Bt
void matmul_seq(const vector<vector<float>>& A,
                const vector<float>& Bt,
                vector<vector<float>>& C,
                int N) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int k = 0; k < N; k++)
                sum += A[i][k] * Bt[j * N + k];
            C[i][j] = sum;
        }
}

// Thread worker
void worker(int s, int e, int N,
            const vector<vector<float>>& A,
            const vector<float>& Bt,
            vector<vector<float>>& C) {
    for (int i = s; i < e; i++)
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int k = 0; k < N; k++)
                sum += A[i][k] * Bt[j * N + k];
            C[i][j] = sum;
        }
}

// Multithreading version
void matmul_thread(const vector<vector<float>>& A,
                   const vector<float>& Bt,
                   vector<vector<float>>& C,
                   int N, int T) {
    vector<thread> th;

    int step = N / T;
    int start = 0;

    for (int t = 0; t < T; t++) {
        int end = (t == T - 1) ? N : start + step;
        th.emplace_back(worker, start, end, N, cref(A), cref(Bt), ref(C));
        start = end;
    }

    for (auto& x : th)
        x.join();
}

// OpenMP version
void matmul_omp(const vector<vector<float>>& A,
                const vector<float>& Bt,
                vector<vector<float>>& C,
                int N, int T) {
#ifdef _OPENMP
    omp_set_num_threads(T);

#pragma omp parallel for collapse(2)
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int k = 0; k < N; k++)
                sum += A[i][k] * Bt[j * N + k];
            C[i][j] = sum;
        }
#else
    matmul_seq(A, Bt, C, N);
#endif
}

// Measure runtime
template<typename F>
double measure(F func) {
    auto s = high_resolution_clock::now();
    func();
    auto e = high_resolution_clock::now();
    return duration<double>(e - s).count();
}

string detect_power_mode()
{
    ifstream f("/sys/class/power_supply/AC0/online");

    if(!f.is_open())
        return "unknown";

    string v;
    getline(f, v);

    if(v == "1")
        return "ac";        // plugged in
    else
        return "battery";   // unplugged
}

int main() {
    const int N = 512;
    const int MAX_T = 15;

    vector<vector<float>> A(N, vector<float>(N));
    vector<vector<float>> B(N, vector<float>(N));
    vector<float> Bt(N * N);

    init_matrix(A, N);
    init_matrix(B, N);
    transpose_flat(B, Bt, N);

    vector<vector<float>> Cseq(N, vector<float>(N));

    double t_seq = measure([&]() {
        matmul_seq(A, Bt, Cseq, N);
    });

    string mode = detect_power_mode();

    string thread_name, openmp_name;
    if (mode == "ac") {
        thread_name = "thread_ac.csv";
        openmp_name = "openmp_ac.csv";
    } else if (mode == "battery") {
        thread_name = "thread_battery.csv";
        openmp_name = "openmp_battery.csv";
    } else {
        thread_name = "thread_unknown.csv";
        openmp_name = "openmp_unknown.csv";
    }

    ofstream f_thread(thread_name);
    ofstream f_openmp(openmp_name);

    if (!f_thread.is_open()) {
        cerr << "Failed to open " << thread_name << "\n";
        return 1;
    }
    if (!f_openmp.is_open()) {
        cerr << "Failed to open " << openmp_name << "\n";
        return 1;
    }

    f_thread << fixed << setprecision(6);
    f_openmp << fixed << setprecision(6);

    f_thread << "N=" << N << "\n";
    f_thread << "t_seq=" << t_seq << "\n";
    f_thread << "threads,t_thread,speedup,efficiency\n";

    f_openmp << "N=" << N << "\n";
    f_openmp << "t_seq=" << t_seq << "\n";
    f_openmp << "threads,t_openmp,speedup,efficiency\n";

    cout << fixed << setprecision(6);
    cout << "Power mode: " << mode << "\n";
    cout << "Sequential Time = " << t_seq << " sec\n\n";

    for (int T = 1; T <= MAX_T; T++) {
        vector<vector<float>> Cthr(N, vector<float>(N));
        vector<vector<float>> Comp(N, vector<float>(N));

        double t_thr = measure([&]() {
            matmul_thread(A, Bt, Cthr, N, T);
        });

        double t_omp = measure([&]() {
            matmul_omp(A, Bt, Comp, N, T);
        });

        double s_thr = t_seq / t_thr;
        double e_thr = s_thr / T;

        double s_omp = t_seq / t_omp;
        double e_omp = s_omp / T;

        f_thread << T << "," << t_thr << "," << s_thr << "," << e_thr << "\n";
        f_openmp << T << "," << t_omp << "," << s_omp << "," << e_omp << "\n";

        cout << "T=" << T
             << " | thread=" << t_thr
             << " | openmp=" << t_omp << "\n";
    }

    f_thread.close();
    f_openmp.close();

    cout << "\nSaved: " << thread_name << ", " << openmp_name << "\n";
    return 0;
}