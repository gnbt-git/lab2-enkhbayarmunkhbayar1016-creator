import csv
from collections import defaultdict
import math
import matplotlib.pyplot as plt

def load_csv(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        r = csv.DictReader(f)
        for row in r:
            # run_i нь 0..9 тоон мөрүүдийг л авна (mean/p95 мөрүүдийг алгасна)
            if row["run_i"].isdigit():
                rows.append({
                    "impl": row["impl"],
                    "threads": int(row["threads"]),
                    "elapsed_ms": float(row["elapsed_ms"])
                })
    return rows

def mean(xs):
    return sum(xs) / len(xs)

def group_mean(rows):
    g = defaultdict(list)
    for x in rows:
        g[(x["impl"], x["threads"])].append(x["elapsed_ms"])
    # mean time per (impl,threads)
    return {k: mean(v) for k, v in g.items()}

def compute_speedup_eff(mean_times):
    # baseline: serial,1
    t1 = mean_times[("serial", 1)]
    sp = {}
    eff = {}
    for (impl, T), t in mean_times.items():
        if impl == "serial" and T == 1:
            sp[(impl, T)] = 1.0
            eff[(impl, T)] = 1.0
        else:
            s = t1 / t
            sp[(impl, T)] = s
            eff[(impl, T)] = s / T
    return sp, eff

def extract_series(metric, impl, maxT):
    # metric dict key = (impl,T)
    Ts = []
    Ys = []
    for T in [1,2,4,8,16]:
        if T > maxT: 
            continue
        k = (impl, T)
        if k in metric:
            Ts.append(T)
            Ys.append(metric[k])
    return Ts, Ys

def main():
    ac = load_csv("results_B_ac.csv")
    ba = load_csv("results_B_battery.csv")

    ac_mean = group_mean(ac)
    ba_mean = group_mean(ba)

    sp_ac, eff_ac = compute_speedup_eff(ac_mean)
    sp_ba, eff_ba = compute_speedup_eff(ba_mean)

    maxT = 16

    # ---- SPEEDUP
    fig, ax = plt.subplots(figsize=(10,5))
    for impl, ls, tag in [
        ("threads", "--", "threads"),
        ("openmp", ":", "openmp"),
    ]:
        T, y = extract_series(sp_ba, impl, maxT)
        ax.plot(T, y, marker="o", linestyle=ls, label=f"{tag} (Battery)")
        T, y = extract_series(sp_ac, impl, maxT)
        ax.plot(T, y, marker="o", linestyle="-", label=f"{tag} (AC)")

    ax.plot([1, maxT], [1, maxT], linestyle="--", label="Ideal (S=T)")
    ax.set_xlabel("Threads")
    ax.set_ylabel("Speedup (T1 / Tn)")
    ax.set_title("Transform: Speedup (Battery vs AC)")
    ax.grid(True)
    ax.legend()
    fig.savefig("speedup_transform.png", dpi=200)

    # ---- EFFICIENCY
    fig, ax = plt.subplots(figsize=(10,5))
    for impl, ls, tag in [
        ("threads", "--", "threads"),
        ("openmp", ":", "openmp"),
    ]:
        T, y = extract_series(eff_ba, impl, maxT)
        ax.plot(T, y, marker="o", linestyle=ls, label=f"{tag} (Battery)")
        T, y = extract_series(eff_ac, impl, maxT)
        ax.plot(T, y, marker="o", linestyle="-", label=f"{tag} (AC)")

    ax.plot([1, maxT], [1, 1], linestyle="--", label="Ideal (E=1)")
    ax.set_xlabel("Threads")
    ax.set_ylabel("Efficiency")
    ax.set_title("Transform: Efficiency (Battery vs AC)")
    ax.grid(True)
    ax.legend()
    fig.savefig("efficiency_transform.png", dpi=200)

    plt.show()
    print("✅ Saved: speedup_transform.png, efficiency_transform.png")

if __name__ == "__main__":
    main()