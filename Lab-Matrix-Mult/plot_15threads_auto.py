import re
from pathlib import Path
import subprocess
import matplotlib.pyplot as plt

MAX_T = 15

def get_lscpu_info():
    keys = [
        "Model name",
        "CPU(s)",
        "Core(s) per socket",
        "Thread(s) per core",
        "L1d cache",
        "L1i cache",
        "L2 cache",
        "L3 cache",
    ]
    info = {k: "?" for k in keys}

    try:
        out = subprocess.check_output(["lscpu"], text=True, stderr=subprocess.DEVNULL)
        for line in out.splitlines():
            if ":" not in line:
                continue
            k, v = [x.strip() for x in line.split(":", 1)]
            if k in info:
                info[k] = v
    except Exception:
        pass

    return info

def read_single_csv(path):
    lines = Path(path).read_text(encoding="utf-8", errors="ignore").strip().splitlines()

    N = None
    t_seq = None
    rows = []

    for line in lines:
        line = line.strip()
        if not line:
            continue

        if line.startswith("N="):
            N = int(line.split("=", 1)[1])
            continue

        if line.startswith("t_seq="):
            t_seq = float(line.split("=", 1)[1])
            continue

        if line.lower().startswith("threads,"):
            continue

        m = re.match(
            r"^\s*(\d+)\s*,\s*([0-9]*\.?[0-9]+)\s*,\s*([0-9]*\.?[0-9]+)\s*,\s*([0-9]*\.?[0-9]+)\s*$",
            line
        )
        if m:
            T = int(m.group(1))
            t_val = float(m.group(2))
            speed = float(m.group(3))
            eff = float(m.group(4))
            rows.append((T, t_val, speed, eff))

    if N is None or t_seq is None:
        raise ValueError(f"{path} файлд N=... эсвэл t_seq=... байхгүй байна.")

    return N, t_seq, rows

def print_separator():
    print("-" * 80)

def print_table(title, headers, rows):
    print(f"\n{title}")
    print_separator()
    print(f"{headers[0]:<10}{headers[1]:<16}{headers[2]:<16}{headers[3]:<16}")
    print_separator()
    for r in rows:
        print(f"{r[0]:<10}{r[1]:<16}{r[2]:<16}{r[3]:<16}")
    print_separator()

def annotate(ax, lscpu, N):
    text = (
        f"CPU: {lscpu['Model name']}\n"
        f"CPU(s): {lscpu['CPU(s)']} | Cores/socket: {lscpu['Core(s) per socket']} | Threads/core: {lscpu['Thread(s) per core']}\n"
        f"L1d: {lscpu['L1d cache']} | L1i: {lscpu['L1i cache']}\n"
        f"L2: {lscpu['L2 cache']} | L3: {lscpu['L3 cache']}\n"
        f"N: {N}"
    )
    ax.text(0.02, -0.34, text, transform=ax.transAxes, ha="left", va="top", fontsize=9)

def main():
    lscpu = get_lscpu_info()

    N1, t_seq_thr_ac, rows_thr_ac = read_single_csv("thread_ac.csv")
    N2, t_seq_omp_ac, rows_omp_ac = read_single_csv("openmp_ac.csv")
    N3, t_seq_thr_ba, rows_thr_ba = read_single_csv("thread_battery.csv")
    N4, t_seq_omp_ba, rows_omp_ba = read_single_csv("openmp_battery.csv")

    print_table(
        "THREAD PLUGGED-IN",
        ["T", "time", "speedup", "efficiency"],
        [[T, f"{t:.6f}", f"{s:.6f}", f"{e:.6f}"] for T, t, s, e in rows_thr_ac]
    )

    print_table(
        "OPENMP PLUGGED-IN",
        ["T", "time", "speedup", "efficiency"],
        [[T, f"{t:.6f}", f"{s:.6f}", f"{e:.6f}"] for T, t, s, e in rows_omp_ac]
    )

    print_table(
        "THREAD BATTERY",
        ["T", "time", "speedup", "efficiency"],
        [[T, f"{t:.6f}", f"{s:.6f}", f"{e:.6f}"] for T, t, s, e in rows_thr_ba]
    )

    print_table(
        "OPENMP BATTERY",
        ["T", "time", "speedup", "efficiency"],
        [[T, f"{t:.6f}", f"{s:.6f}", f"{e:.6f}"] for T, t, s, e in rows_omp_ba]
    )

    # Speedup graph
    fig, ax = plt.subplots(figsize=(11, 5.5))

    ax.plot([r[0] for r in rows_thr_ac], [r[2] for r in rows_thr_ac], marker="o", label="Thread Plugged-in")
    ax.plot([r[0] for r in rows_omp_ac], [r[2] for r in rows_omp_ac], marker="o", label="OpenMP Plugged-in")
    ax.plot([r[0] for r in rows_thr_ba], [r[2] for r in rows_thr_ba], marker="o", linestyle="--", label="Thread Battery")
    ax.plot([r[0] for r in rows_omp_ba], [r[2] for r in rows_omp_ba], marker="o", linestyle="--", label="OpenMP Battery")

    ax.plot([1, MAX_T], [1, MAX_T], linestyle="--", label="Ideal")
    ax.set_xlabel("Threads")
    ax.set_ylabel("Speedup")
    ax.set_title("Speedup Comparison")
    ax.grid(True)
    ax.legend()
    annotate(ax, lscpu, N1)
    fig.subplots_adjust(bottom=0.35)
    fig.savefig("speedup_compare.png", dpi=200)

    # Efficiency graph
    fig, ax = plt.subplots(figsize=(11, 5.5))

    ax.plot([r[0] for r in rows_thr_ac], [r[3] for r in rows_thr_ac], marker="o", label="Thread Plugged-in")
    ax.plot([r[0] for r in rows_omp_ac], [r[3] for r in rows_omp_ac], marker="o", label="OpenMP Plugged-in")
    ax.plot([r[0] for r in rows_thr_ba], [r[3] for r in rows_thr_ba], marker="o", linestyle="--", label="Thread Battery")
    ax.plot([r[0] for r in rows_omp_ba], [r[3] for r in rows_omp_ba], marker="o", linestyle="--", label="OpenMP Battery")

    ax.plot([1, MAX_T], [1, 1], linestyle="--", label="Ideal")
    ax.set_xlabel("Threads")
    ax.set_ylabel("Efficiency")
    ax.set_title("Efficiency Comparison")
    ax.grid(True)
    ax.legend()
    annotate(ax, lscpu, N1)
    fig.subplots_adjust(bottom=0.35)
    fig.savefig("efficiency_compare.png", dpi=200)

    plt.show()

    print("\nSaved:")
    print("- speedup_compare.png")
    print("- efficiency_compare.png")

if __name__ == "__main__":
    main()