import json
import time
import os
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np
import threading

RESULTS_FILE = "results.json"
POLL_INTERVAL = 0.3

COLORS = {
    "Serial":   "#FF5F5F",
    "Spawn":    "#00DBDE",
    "Spinning": "#FC00FF",
    "Sleeping": "#00FF87",
}
ALL_SYSTEMS = ["Serial", "Spawn", "Spinning", "Sleeping"]

# Thread тоог авах
THREAD_COUNTS = {
    "Serial":   1,
    "Spawn":    threading.active_count(),
    "Spinning": os.cpu_count(),
    "Sleeping": os.cpu_count(),
}

def read_results():
    if not os.path.exists(RESULTS_FILE):
        return {}
    try:
        with open(RESULTS_FILE, "r") as f:
            data = json.load(f)
        results = {}
        for entry in data:
            results[entry["name"]] = entry["time_ms"]
            # JSON дотор thread_count байвал авна
            if "thread_count" in entry:
                THREAD_COUNTS[entry["name"]] = entry["thread_count"]
        return results
    except Exception:
        return {}

plt.style.use('dark_background')
fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(20, 6))
fig.patch.set_facecolor('#121212')
fig.suptitle("📊 Live Task System Benchmark — Waiting for C++ results...",
             color='white', fontsize=13)

def update(frame):
    results = read_results()

    names   = [s for s in ALL_SYSTEMS if s in results]
    times   = [results[s] for s in names]
    colors  = [COLORS[s] for s in names]
    threads = [THREAD_COUNTS.get(s, 1) for s in names]

    if not times:
        return

    serial_time = results.get("Serial", times[0])
    speedups = [serial_time / t for t in times]

    # ── Left: Speedup Bar ──
    ax1.clear()
    ax1.set_facecolor('#1a1a1a')
    bars = ax1.bar(names, speedups, color=colors, alpha=0.85,
                   edgecolor='white', linewidth=0.5)
    ax1.set_title("Speedup Factor (Higher is Better)",
                  color="#00FF87", fontsize=13, pad=12)
    ax1.set_ylabel("Times faster than Serial", color='white')
    ax1.axhline(y=1, color='white', linestyle='--', alpha=0.3)
    ax1.set_ylim(0, max(speedups) * 1.25 if speedups else 2)
    ax1.tick_params(colors='white')
    for bar, sp in zip(bars, speedups):
        ax1.text(bar.get_x() + bar.get_width() / 2.,
                 bar.get_height() + 0.05,
                 f'{sp:.2f}x',
                 ha='center', weight='bold', color='white', fontsize=12)

    # ── Middle: Execution Time ──
    ax2.clear()
    ax2.set_facecolor('#1a1a1a')
    x_idx = list(range(len(names)))
    ax2.plot(x_idx, times, marker='o', color='#00DBDE',
             linewidth=2.5, markersize=10, markerfacecolor='white',
             markeredgecolor='#00DBDE', markeredgewidth=2)
    ax2.set_title("Execution Time (Lower is Better)",
                  color="#FF5F5F", fontsize=13, pad=12)
    ax2.set_ylabel("Milliseconds", color='white')
    ax2.set_xticks(x_idx)
    ax2.set_xticklabels(names, color='white')
    ax2.tick_params(colors='white')
    ax2.grid(axis='y', linestyle=':', alpha=0.3)
    ax2.set_ylim(0, max(times) * 1.35 if times else 10)
    for i, (v, n) in enumerate(zip(times, names)):
        ax2.text(i, v + max(times) * 0.04,
                 f'{v:.2f}ms',
                 ha='center', color=COLORS[n], fontsize=10, weight='bold')

    # ── Right: Thread Count Bar ──
    ax3.clear()
    ax3.set_facecolor('#1a1a1a')
    t_bars = ax3.bar(names, threads, color=colors, alpha=0.85,
                     edgecolor='white', linewidth=0.5)
    ax3.set_title("Thread Count per System",
                  color="#00DBDE", fontsize=13, pad=12)
    ax3.set_ylabel("Number of Threads", color='white')
    ax3.tick_params(colors='white')
    ax3.set_ylim(0, max(threads) * 1.35 if threads else 4)
    ax3.grid(axis='y', linestyle=':', alpha=0.3)
    for bar, t in zip(t_bars, threads):
        ax3.text(bar.get_x() + bar.get_width() / 2.,
                 bar.get_height() + 0.1,
                 f'{t} threads',
                 ha='center', weight='bold', color='white', fontsize=11)

    # ── Status ──
    completed = len(results)
    total     = len(ALL_SYSTEMS)
    status    = "✅ All done!" if completed == total else f"⏳ Running... ({completed}/{total})"
    best = min(results, key=results.get) if results else ""
    fig.suptitle(
        f"📊 Live Task System Benchmark  |  {status}"
        + (f"  |  🏆 Fastest: {best}" if completed == total else ""),
        color='white', fontsize=12
    )

    if completed == total:
        fig.savefig("performance_report.png", dpi=120, bbox_inches='tight',
                    facecolor='#121212')

    plt.tight_layout(rect=[0, 0, 1, 0.95])

ani = animation.FuncAnimation(fig, update, interval=int(POLL_INTERVAL * 1000),
                               cache_frame_data=False)

print("🚀 Python chart is LIVE — Now run your C++ benchmark:")
print("   ./main")
print("")
print("Chart will update automatically as each system finishes.")
print("Close the window to exit.\n")

plt.tight_layout(rect=[0, 0, 1, 0.95])
plt.show()