import re
import subprocess
from pathlib import Path
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

def read_bench_csv(path: str):
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

        m = re.match(r"^\s*(\d+)\s*,\s*([0-9]*\.?[0-9]+)\s*,\s*([0-9]*\.?[0-9]+)\s*$", line)
        if m:
            T = int(m.group(1))
            t_thr = float(m.group(2))
            t_omp = float(m.group(3))
            if 1 <= T <= MAX_T:
                rows.append((T, t_thr, t_omp))

    if N is None or t_seq is None:
        raise ValueError(f"{path} файлд N=... эсвэл t_seq=... мөр байхгүй байна.")

    rows.sort(key=lambda x: x[0])

    if len(rows) == 0:
        raise ValueError(f"{path} файлд thread мөрүүд уншигдсангүй.")

    return N, t_seq, rows

def speedup(tseq, tpar):
    return tseq / tpar

def efficiency(sp, T):
    return sp / T

def annotate(ax, lscpu, N, power_note):
    text = (
        f"CPU: {lscpu['Model name']}\n"
        f"CPU(s): {lscpu['CPU(s)']} | Cores/socket: {lscpu['Core(s) per socket']} | Threads/core: {lscpu['Thread(s) per core']}\n"
        f"L1d: {lscpu['L1d cache']} | L1i: {lscpu['L1i cache']}\n"
        f"L2: {lscpu['L2 cache']} | L3: {lscpu['L3 cache']}\n"
        f"N: {N}\n"
        f"Power: {power_note}"
    )
    ax.text(0.02, -0.34, text, transform=ax.transAxes, ha="left", va="top", fontsize=9)

def print_separator():
    print("-" * 120)

def print_table(title, headers, rows):
    print(f"\n{title}")
    print_separator()
    print(
        f"{headers[0]:<8}"
        f"{headers[1]:<14}"
        f"{headers[2]:<14}"
        f"{headers[3]:<14}"
        f"{headers[4]:<18}"
        f"{headers[5]:<18}"
        f"{headers[6]:<16}"
        f"{headers[7]:<16}"
    )
    print_separator()

    for row in rows:
        print(
            f"{row[0]:<8}"
            f"{row[1]:<14}"
            f"{row[2]:<14}"
            f"{row[3]:<14}"
            f"{row[4]:<18}"
            f"{row[5]:<18}"
            f"{row[6]:<16}"
            f"{row[7]:<16}"
        )
    print_separator()

def build_full_table(t_seq, rows):
    table = []
    for T, t_thr, t_omp in rows:
        sp_thr = speedup(t_seq, t_thr)
        sp_omp = speedup(t_seq, t_omp)

        eff_thr = efficiency(sp_thr, T)
        eff_omp = efficiency(sp_omp, T)

        table.append([
            T,
            f"{t_seq:.6f}",
            f"{t_thr:.6f}",
            f"{t_omp:.6f}",
            f"{sp_thr:.6f}",
            f"{sp_omp:.6f}",
            f"{eff_thr:.6f}",
            f"{eff_omp:.6f}",
        ])
    return table

def save_table_csv(path, table):
    with open(path, "w", encoding="utf-8") as f:
        f.write("T,t_seq,t_thread,t_openmp,speedup_thread,speedup_openmp,eff_thread,eff_openmp\n")
        for row in table:
            f.write(",".join(map(str, row)) + "\n")

def main():
    lscpu = get_lscpu_info()

    N_ac, t_seq_ac, rows_ac = read_bench_csv("bench_ac.csv")
    N_ba, t_seq_ba, rows_ba = read_bench_csv("bench_battery.csv")

    table_ac = build_full_table(t_seq_ac, rows_ac)
    table_ba = build_full_table(t_seq_ba, rows_ba)

    print(f"\nCPU INFO: {lscpu['Model name']}")
    print(f"N = {N_ac}")

    print_table(
        "PLUGGED-IN FULL TABLE",
        ["T", "t_seq", "t_thread", "t_openmp", "speedup_thread", "speedup_openmp", "eff_thread", "eff_openmp"],
        table_ac
    )

    print_table(
        "BATTERY FULL TABLE",
        ["T", "t_seq", "t_thread", "t_openmp", "speedup_thread", "speedup_openmp", "eff_thread", "eff_openmp"],
        table_ba
    )

    save_table_csv("table_ac.csv", table_ac)
    save_table_csv("table_battery.csv", table_ba)

    # -------- graphs --------
    T_ac = [r[0] for r in rows_ac]
    t_thr_ac = [r[1] for r in rows_ac]
    t_omp_ac = [r[2] for r in rows_ac]

    T_ba = [r[0] for r in rows_ba]
    t_thr_ba = [r[1] for r in rows_ba]
    t_omp_ba = [r[2] for r in rows_ba]

    sp_thr_ac = [speedup(t_seq_ac, t) for t in t_thr_ac]
    sp_omp_ac = [speedup(t_seq_ac, t) for t in t_omp_ac]
    sp_thr_ba = [speedup(t_seq_ba, t) for t in t_thr_ba]
    sp_omp_ba = [speedup(t_seq_ba, t) for t in t_omp_ba]

    eff_thr_ac = [efficiency(sp, T) for sp, T in zip(sp_thr_ac, T_ac)]
    eff_omp_ac = [efficiency(sp, T) for sp, T in zip(sp_omp_ac, T_ac)]
    eff_thr_ba = [efficiency(sp, T) for sp, T in zip(sp_thr_ba, T_ba)]
    eff_omp_ba = [efficiency(sp, T) for sp, T in zip(sp_omp_ba, T_ba)]

    power_note = "Battery vs Plugged-in"

    fig, ax = plt.subplots(figsize=(11, 5.5))
    ax.plot(T_ba, sp_thr_ba, marker="o", linestyle="--", label="threads (Battery)")
    ax.plot(T_ba, sp_omp_ba, marker="o", linestyle=":", label="OpenMP (Battery)")
    ax.plot(T_ac, sp_thr_ac, marker="o", label="threads (Plugged-in)")
    ax.plot(T_ac, sp_omp_ac, marker="o", label="OpenMP (Plugged-in)")
    ax.plot([1, MAX_T], [1, MAX_T], linestyle="--", label="Ideal (S=T)")
    ax.set_xlabel("Threads (1–15)")
    ax.set_ylabel("Speedup")
    ax.set_title("matmul: Speedup (Battery vs Plugged-in)")
    ax.grid(True)
    ax.legend()
    annotate(ax, lscpu, N_ac, power_note)
    fig.subplots_adjust(bottom=0.35)
    fig.savefig("speedup_1_15.png", dpi=200)

    fig, ax = plt.subplots(figsize=(11, 5.5))
    ax.plot(T_ba, eff_thr_ba, marker="o", linestyle="--", label="threads (Battery)")
    ax.plot(T_ba, eff_omp_ba, marker="o", linestyle=":", label="OpenMP (Battery)")
    ax.plot(T_ac, eff_thr_ac, marker="o", label="threads (Plugged-in)")
    ax.plot(T_ac, eff_omp_ac, marker="o", label="OpenMP (Plugged-in)")
    ax.plot([1, MAX_T], [1, 1], linestyle="--", label="Ideal (E=1)")
    ax.set_xlabel("Threads (1–15)")
    ax.set_ylabel("Efficiency")
    ax.set_title("matmul: Efficiency (Battery vs Plugged-in)")
    ax.grid(True)
    ax.legend()
    annotate(ax, lscpu, N_ac, power_note)
    fig.subplots_adjust(bottom=0.35)
    fig.savefig("efficiency_1_15.png", dpi=200)

    plt.show()

    print("\n✅ Saved:")
    print("table_ac.csv")
    print("table_battery.csv")
    print("speedup_1_15.png")
    print("efficiency_1_15.png")

if __name__ == "__main__":
    main()