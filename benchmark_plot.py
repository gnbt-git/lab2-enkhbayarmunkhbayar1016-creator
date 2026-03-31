import json
import os
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.animation as animation
import matplotlib.patheffects as pe
import numpy as np
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch

RESULTS_FILE = "results.json"
POLL_INTERVAL = 0.3
NUM_THREADS = 4
NUM_TASKS = 5000
TASKS_PER_THREAD = NUM_TASKS // NUM_THREADS  # 1250

COLORS = {
    "Serial":   "#FF5F5F",
    "Spawn":    "#00DBDE",
    "Spinning": "#FC00FF",
    "Sleeping": "#00FF87",
}
THREAD_COLORS = ["#00DBDE", "#00FF87", "#FF5F5F", "#FC00FF"]
ALL_SYSTEMS = ["Serial", "Spawn", "Spinning", "Sleeping"]

def read_results():
    if not os.path.exists(RESULTS_FILE):
        return {}
    try:
        with open(RESULTS_FILE) as f:
            data = json.load(f)
        return {e["name"]: e for e in data}
    except:
        return {}

plt.style.use('dark_background')
fig = plt.figure(figsize=(24, 13))
fig.patch.set_facecolor('#0a0a0a')

gs = fig.add_gridspec(2, 3, hspace=0.45, wspace=0.35,
                      left=0.05, right=0.97, top=0.92, bottom=0.06)

ax_fork   = fig.add_subplot(gs[0, 0])   # Fork/Join (Spinning diagram)
ax_sleep  = fig.add_subplot(gs[0, 1])   # Thread Sleep diagram
ax_cmp    = fig.add_subplot(gs[0, 2])   # Spinning vs Sleeping comparison
ax_speed  = fig.add_subplot(gs[1, 0])   # Speedup
ax_time   = fig.add_subplot(gs[1, 1])   # Time
ax_prog   = fig.add_subplot(gs[1, 2])   # Progress bars (live)

spin_progress  = [0] * NUM_THREADS
sleep_progress = [0] * NUM_THREADS

def simulate_step():
    total_spin = sum(spin_progress)
    for t in range(NUM_THREADS):
        # Spinning: 4-р thread 800 task дуустал хүлээнэ
        if t == 3 and total_spin < 800:
            pass
        elif spin_progress[t] < TASKS_PER_THREAD:
            spin_progress[t] = min(TASKS_PER_THREAD,
                                   spin_progress[t] + np.random.randint(6, 20))
        # Sleeping: бүгд нэгэн зэрэг
        if sleep_progress[t] < TASKS_PER_THREAD:
            sleep_progress[t] = min(TASKS_PER_THREAD,
                                    sleep_progress[t] + np.random.randint(12, 30))

# ══════════════════════════════════════════════════════
# DIAGRAM 1 — Fork/Join (Spinning / Parallel Spawn)
# ══════════════════════════════════════════════════════
def draw_fork_join(ax):
    ax.clear()
    ax.set_facecolor('#111111')
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 6)
    ax.axis('off')
    ax.set_title("Spinning — fork / join", color=COLORS["Spinning"],
                 fontsize=12, fontweight='bold', pad=8)

    # Master thread horizontal line
    ax.annotate('', xy=(9.5, 3), xytext=(0.2, 3),
                arrowprops=dict(arrowstyle='->', color='#888', lw=1.5))
    ax.text(0.1, 3.2, 'Master\nThread', color='#aaa', fontsize=8, ha='left')

    # 3 parallel task groups
    task_x = [2.5, 5.5, 8.2]
    task_labels = [
        ['A', 'B', 'C'],
        ['A', 'B', 'C', 'D'],
        ['A', 'B'],
    ]
    task_colors_map = {
        'A': '#3ddc84', 'B': '#7b61ff',
        'C': '#ff4444', 'D': '#ffa500'
    }
    titles = ['Parallel Task I', 'Parallel Task II', 'Parallel Task III']

    for gi, (cx, labels, title) in enumerate(zip(task_x, task_labels, titles)):
        n = len(labels)
        ys = np.linspace(1.0, 5.0, n)
        spread = (max(ys) - min(ys)) / 2 + 0.5
        center_y = 3.0

        # Dashed container
        rect = FancyBboxPatch((cx - 0.9, center_y - spread - 0.3),
                              1.8, 2 * spread + 0.6,
                              boxstyle="round,pad=0.05",
                              linewidth=0.8, linestyle='--',
                              edgecolor='#6655cc', facecolor='#1a1a2e', zorder=1)
        ax.add_patch(rect)
        ax.text(cx, center_y + spread + 0.5, title,
                ha='center', va='center', color='#ccbbff', fontsize=7.5)

        # Fork point on master line
        fork_x = cx - 0.75
        ax.plot(fork_x, 3, 'o', color='#888', markersize=5, zorder=3)

        for y, lbl in zip(ys, labels):
            col = task_colors_map.get(lbl, '#aaa')
            # Arrow from fork
            ax.annotate('', xy=(cx - 0.45, y), xytext=(fork_x, 3),
                        arrowprops=dict(arrowstyle='->', color=col, lw=1))
            # Task box
            bp = FancyBboxPatch((cx - 0.42, y - 0.22), 0.84, 0.44,
                                boxstyle="round,pad=0.05",
                                facecolor=col, edgecolor='white',
                                linewidth=0.5, zorder=4)
            ax.add_patch(bp)
            ax.text(cx, y, lbl, ha='center', va='center',
                    color='black', fontsize=9, fontweight='bold', zorder=5)

            # Arrow to join
            join_x = cx + 0.75
            ax.annotate('', xy=(join_x, 3), xytext=(cx + 0.42, y),
                        arrowprops=dict(arrowstyle='->', color=col, lw=1))

        # Join point
        ax.plot(cx + 0.75, 3, 'o', color='#888', markersize=5, zorder=3)

# ══════════════════════════════════════════════════════
# DIAGRAM 2 — Thread Pool Sleeping
# ══════════════════════════════════════════════════════
def draw_sleep_diagram(ax):
    ax.clear()
    ax.set_facecolor('#111111')
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 6.5)
    ax.axis('off')
    ax.set_title("Sleeping — thread pool + cv::wait", color=COLORS["Sleeping"],
                 fontsize=12, fontweight='bold', pad=8)

    t_colors = THREAD_COLORS
    thread_ys = [5.0, 3.8, 2.6, 1.4]
    thread_names = ['Thread 0', 'Thread 1', 'Thread 2', 'Thread 3']

    # ── Memory block ──
    mem_rect = FancyBboxPatch((0.1, 0.8), 1.2, 5.0,
                              boxstyle="round,pad=0.05",
                              facecolor='#1e1e2e', edgecolor='#555', lw=0.8)
    ax.add_patch(mem_rect)
    ax.text(0.7, 6.0, 'Memory', ha='center', color='#aaa', fontsize=9, fontweight='bold')

    for i, (ty, tc, tn) in enumerate(zip(thread_ys, t_colors, thread_names)):
        ax.text(-0.05, ty, tn, ha='right', va='center', color=tc, fontsize=8)
        ax.text(0.7, ty + 0.18, 'ltb', ha='center', va='center', color='#ccc', fontsize=7)
        ax.text(0.7, ty - 0.18, 'utb', ha='center', va='center', color='#ccc', fontsize=7)
        if i < 3:
            ax.plot([0.15, 1.25], [ty - 0.35, ty - 0.35],
                    color='#333', lw=0.5)

    # ── Driver box ──
    drv = FancyBboxPatch((1.6, 2.5), 1.0, 1.2,
                         boxstyle="round,pad=0.05",
                         facecolor='#2a2a1a', edgecolor='#aa9900', lw=1)
    ax.add_patch(drv)
    ax.text(2.1, 3.1, 'Driver', ha='center', va='center',
            color='#ffcc00', fontsize=9, fontweight='bold')

    # Fan-out arrows Driver → subroutine 1
    sub1_x = 4.2
    for ty, tc in zip(thread_ys, t_colors):
        ax.annotate('', xy=(sub1_x - 0.05, ty), xytext=(2.6, 3.1),
                    arrowprops=dict(arrowstyle='->', color=tc, lw=1.0))

    # ── Subroutine 1 ──
    sub1_rect = FancyBboxPatch((sub1_x, 0.8), 2.2, 5.0,
                               boxstyle="round,pad=0.05",
                               facecolor='#111122', edgecolor='#6655cc',
                               lw=0.8, linestyle='--')
    ax.add_patch(sub1_rect)
    ax.text(sub1_x + 1.1, 6.0, 'Subroutine', ha='center',
            color='#ccbbff', fontsize=8.5, fontweight='bold')

    for ty, tc, tn in zip(thread_ys, t_colors, thread_names):
        box = FancyBboxPatch((sub1_x + 0.1, ty - 0.3), 2.0, 0.6,
                             boxstyle="round,pad=0.04",
                             facecolor='#1e1e3e', edgecolor=tc, lw=0.8)
        ax.add_patch(box)
        ax.text(sub1_x + 1.1, ty + 0.05, tn, ha='center', va='center',
                color=tc, fontsize=7.5, fontweight='bold')
        ax.text(sub1_x + 1.1, ty - 0.15, 'do i = ltb, utb', ha='center',
                va='center', color='#aaa', fontsize=6.5)

    # Fan-in → serial section
    ser_x = 7.0
    for ty, tc in zip(thread_ys, t_colors):
        ax.annotate('', xy=(ser_x, 3.1), xytext=(sub1_x + 2.2, ty),
                    arrowprops=dict(arrowstyle='->', color=tc, lw=1.0))

    # "Dominant threads" label
    ax.text(6.85, 3.1, 'Dominant\nthreads', ha='center', va='center',
            color='#777', fontsize=7, rotation=90)

    # ── Serial section ──
    ser_rect = FancyBboxPatch((7.1, 2.3), 1.4, 1.6,
                              boxstyle="round,pad=0.05",
                              facecolor='#1a1a1a', edgecolor='#777', lw=0.8)
    ax.add_patch(ser_rect)
    ax.text(7.8, 3.2, 'Serial\nsection', ha='center', va='center',
            color='#bbb', fontsize=8.5)

    # Arrow serial → subroutine 2
    sub2_x = 8.8
    ax.annotate('', xy=(sub2_x, 3.1), xytext=(8.5, 3.1),
                arrowprops=dict(arrowstyle='->', color='#777', lw=1.0))

    # ── Subroutine 2 ──
    sub2_rect = FancyBboxPatch((sub2_x, 0.8), 2.2, 5.0,
                               boxstyle="round,pad=0.05",
                               facecolor='#111122', edgecolor='#6655cc',
                               lw=0.8, linestyle='--')
    ax.add_patch(sub2_rect)
    ax.text(sub2_x + 1.1, 6.0, 'Subroutine', ha='center',
            color='#ccbbff', fontsize=8.5, fontweight='bold')

    for ty, tc, tn in zip(thread_ys, t_colors, thread_names):
        box2 = FancyBboxPatch((sub2_x + 0.1, ty - 0.3), 2.0, 0.6,
                              boxstyle="round,pad=0.04",
                              facecolor='#1e1e3e', edgecolor=tc, lw=0.8)
        ax.add_patch(box2)
        ax.text(sub2_x + 1.1, ty + 0.05, tn, ha='center', va='center',
                color=tc, fontsize=7.5, fontweight='bold')
        ax.text(sub2_x + 1.1, ty - 0.15, 'do i = ltb, utb', ha='center',
                va='center', color='#aaa', fontsize=6.5)

    # Fan-in → Results
    res_x = 11.3
    for ty, tc in zip(thread_ys, t_colors):
        ax.annotate('', xy=(res_x - 0.1, 3.1), xytext=(sub2_x + 2.2, ty),
                    arrowprops=dict(arrowstyle='->', color=tc, lw=1.0))

    # ── Results box ──
    res_rect = FancyBboxPatch((res_x, 2.5), 0.65, 1.2,
                              boxstyle="round,pad=0.05",
                              facecolor='#0a2a0a', edgecolor='#00FF87', lw=1.2)
    ax.add_patch(res_rect)
    ax.text(res_x + 0.32, 3.1, 'Results', ha='center', va='center',
            color='#00FF87', fontsize=7.5, fontweight='bold')

    # cv::wait annotation
    ax.text(5.3, 0.3,
            'cv_work.wait()  →  thread унтдаг, OS сэрээдэг  →  cv_work.notify_all()',
            ha='center', color='#00FF87', fontsize=7.5, style='italic')

# ══════════════════════════════════════════════════════
# DIAGRAM 3 — Spinning vs Sleeping comparison bars
# ══════════════════════════════════════════════════════
def draw_comparison(ax, results):
    ax.clear()
    ax.set_facecolor('#1a1a1a')
    ax.set_title("Spinning vs Sleeping", color='white', fontsize=12, pad=8)

    spin_ms  = results["Spinning"]["time_ms"] if "Spinning" in results else None
    sleep_ms = results["Sleeping"]["time_ms"] if "Sleeping" in results else None

    if spin_ms and sleep_ms:
        vals = [spin_ms, sleep_ms]
        cols = [COLORS["Spinning"], COLORS["Sleeping"]]
        cats = ['Spinning\n(busy wait)', 'Sleeping\n(cv::wait)']
        bars = ax.bar(cats, vals, color=cols, alpha=0.85,
                      edgecolor='white', linewidth=0.5, width=0.45)
        ax.set_ylim(0, max(vals) * 1.4)
        ax.set_ylabel("Time (ms)", color='white')
        ax.tick_params(colors='white')
        ax.grid(axis='y', linestyle=':', alpha=0.25)
        for bar, val in zip(bars, vals):
            ax.text(bar.get_x() + bar.get_width()/2.,
                    bar.get_height() + max(vals)*0.03,
                    f'{val:.2f} ms',
                    ha='center', color='white', fontsize=11, fontweight='bold')
        diff = spin_ms - sleep_ms
        sign = "faster" if diff > 0 else "slower"
        col2 = '#00FF87' if diff > 0 else '#FF5F5F'
        ax.text(0.5, 0.92,
                f'Sleeping {abs(diff):.1f} ms {sign}',
                ha='center', transform=ax.transAxes,
                color=col2, fontsize=11, fontweight='bold')

        # CPU overhead annotation
        ax.annotate('CPU wasted\n(busy loop)',
                    xy=(0, spin_ms), xytext=(0.25, spin_ms * 0.6),
                    color=COLORS["Spinning"], fontsize=8,
                    arrowprops=dict(arrowstyle='->', color=COLORS["Spinning"]))
        ax.annotate('OS managed\n(efficient)',
                    xy=(1, sleep_ms), xytext=(0.75, sleep_ms * 1.2),
                    color=COLORS["Sleeping"], fontsize=8,
                    arrowprops=dict(arrowstyle='->', color=COLORS["Sleeping"]))
    else:
        ax.text(0.5, 0.5, 'Waiting for data...',
                ha='center', va='center', color='#555',
                transform=ax.transAxes, fontsize=12)

# ══════════════════════════════════════════════════════
# DIAGRAM 4 — Speedup bars
# ══════════════════════════════════════════════════════
def draw_speedup(ax, results):
    ax.clear()
    ax.set_facecolor('#1a1a1a')
    ax.set_title("Speedup factor  (higher = better)", color="#00FF87",
                 fontsize=11, pad=8)

    names = [s for s in ALL_SYSTEMS if s in results]
    if not names:
        ax.text(0.5, 0.5, 'Waiting...', ha='center', va='center',
                color='#555', transform=ax.transAxes)
        return

    times    = [results[s]["time_ms"] for s in names]
    serial_t = results.get("Serial", {}).get("time_ms", times[0])
    speedups = [serial_t / t for t in times]
    colors   = [COLORS[s] for s in names]

    bars = ax.bar(names, speedups, color=colors, alpha=0.85,
                  edgecolor='white', linewidth=0.5)
    ax.set_ylim(0, max(speedups) * 1.3)
    ax.axhline(y=1, color='white', linestyle='--', alpha=0.3)
    ax.set_ylabel("× faster than serial", color='white')
    ax.tick_params(colors='white')
    ax.grid(axis='y', linestyle=':', alpha=0.2)
    for bar, sp in zip(bars, speedups):
        ax.text(bar.get_x() + bar.get_width()/2.,
                bar.get_height() + 0.04,
                f'{sp:.2f}×',
                ha='center', fontweight='bold', color='white', fontsize=11)

# ══════════════════════════════════════════════════════
# DIAGRAM 5 — Execution time line
# ══════════════════════════════════════════════════════
def draw_time(ax, results):
    ax.clear()
    ax.set_facecolor('#1a1a1a')
    ax.set_title("Execution time  (lower = better)", color="#FF5F5F",
                 fontsize=11, pad=8)

    names = [s for s in ALL_SYSTEMS if s in results]
    if not names:
        ax.text(0.5, 0.5, 'Waiting...', ha='center', va='center',
                color='#555', transform=ax.transAxes)
        return

    times  = [results[s]["time_ms"] for s in names]
    colors = [COLORS[s] for s in names]

    x_idx = list(range(len(names)))
    ax.plot(x_idx, times, marker='o', color='#00DBDE',
            linewidth=2.5, markersize=10,
            markerfacecolor='white', markeredgecolor='#00DBDE', markeredgewidth=2)
    ax.set_xticks(x_idx)
    ax.set_xticklabels(names, color='white')
    ax.set_ylabel("Milliseconds", color='white')
    ax.tick_params(colors='white')
    ax.grid(axis='y', linestyle=':', alpha=0.25)
    ax.set_ylim(0, max(times) * 1.4)
    for i, (v, n) in enumerate(zip(times, names)):
        ax.text(i, v + max(times)*0.05,
                f'{v:.1f} ms',
                ha='center', color=COLORS[n], fontsize=10, fontweight='bold')

# ══════════════════════════════════════════════════════
# DIAGRAM 6 — Live progress bars per thread
# ══════════════════════════════════════════════════════
def draw_progress(ax, spin_prog, sleep_prog, results):
    ax.clear()
    ax.set_facecolor('#1a1a1a')
    ax.set_title(f"Thread progress  ({NUM_TASKS} tasks / {NUM_THREADS} threads)",
                 color='white', fontsize=11, pad=8)
    ax.set_xlim(0, TASKS_PER_THREAD * 2.4)
    ax.set_ylim(-0.5, NUM_THREADS * 2 - 0.5)
    ax.axis('off')

    for t in range(NUM_THREADS):
        tc = THREAD_COLORS[t]
        sp = spin_prog[t]
        sl = sleep_prog[t]

        # Spinning row
        y_spin = t * 2 + 1.0
        ax.barh(y_spin, sp, height=0.5, color=COLORS["Spinning"],
                alpha=0.7, left=0)
        ax.barh(y_spin, TASKS_PER_THREAD - sp, height=0.5,
                color='#333', alpha=0.4, left=sp)
        spin_status = "WAITING" if (t == 3 and sum(spin_prog) < 800) else f"{sp}/{TASKS_PER_THREAD}"
        ax.text(sp + 20, y_spin, spin_status,
                va='center', color=COLORS["Spinning"], fontsize=8)
        ax.text(-20, y_spin, f'T{t} spin', va='center', ha='right',
                color=COLORS["Spinning"], fontsize=8)

        # Sleeping row
        y_sleep = t * 2 + 0.35
        ax.barh(y_sleep, sl, height=0.5, color=COLORS["Sleeping"],
                alpha=0.7, left=TASKS_PER_THREAD * 1.3)
        ax.barh(y_sleep, TASKS_PER_THREAD - sl, height=0.5,
                color='#333', alpha=0.4, left=TASKS_PER_THREAD * 1.3 + sl)
        ax.text(TASKS_PER_THREAD * 1.3 + sl + 20, y_sleep,
                f'{sl}/{TASKS_PER_THREAD}',
                va='center', color=COLORS["Sleeping"], fontsize=8)
        ax.text(TASKS_PER_THREAD * 1.28, y_sleep, f'T{t} sleep',
                va='center', ha='right', color=COLORS["Sleeping"], fontsize=8)

    # Legend
    ax.text(TASKS_PER_THREAD * 0.5, -0.4, 'Spinning',
            ha='center', color=COLORS["Spinning"], fontsize=9, fontweight='bold')
    ax.text(TASKS_PER_THREAD * 1.8, -0.4, 'Sleeping',
            ha='center', color=COLORS["Sleeping"], fontsize=9, fontweight='bold')

    if sum(spin_prog) < 800:
        ax.text(TASKS_PER_THREAD * 0.5, NUM_THREADS * 2 - 0.2,
                'T3 waiting (< 800 done)',
                ha='center', color='yellow', fontsize=8)

# ══════════════════════════════════════════════════════
# MAIN ANIMATION UPDATE
# ══════════════════════════════════════════════════════
def update(frame):
    results = read_results()
    simulate_step()

    # Override with real data if available
    if "Spinning" in results:
        tc = results["Spinning"].get("thread_counts", [])
        for i, v in enumerate(tc[:NUM_THREADS]):
            spin_progress[i] = v

    if "Sleeping" in results:
        tc = results["Sleeping"].get("thread_counts", [])
        for i, v in enumerate(tc[:NUM_THREADS]):
            sleep_progress[i] = v

    draw_fork_join(ax_fork)
    draw_sleep_diagram(ax_sleep)
    draw_comparison(ax_cmp, results)
    draw_speedup(ax_speed, results)
    draw_time(ax_time, results)
    draw_progress(ax_prog, spin_progress, sleep_progress, results)

    completed = len(results)
    total     = len(ALL_SYSTEMS)
    status    = "All done!" if completed == total else f"{completed}/{total} running..."
    best = min(results, key=lambda k: results[k]["time_ms"]) if results else ""
    fig.suptitle(
        f"Task System Benchmark  |  {NUM_TASKS} tasks  |  {NUM_THREADS} threads  "
        f"|  {TASKS_PER_THREAD} per thread  |  {status}"
        + (f"  |  Fastest: {best}" if completed == total else ""),
        color='white', fontsize=13, y=0.97, fontweight='bold'
    )

    if completed == total:
        fig.savefig("performance_report.png", dpi=120,
                    bbox_inches='tight', facecolor='#0a0a0a')

ani = animation.FuncAnimation(fig, update,
                               interval=int(POLL_INTERVAL * 1000),
                               cache_frame_data=False)

print(f"Live benchmark chart started!")
print(f"  {NUM_TASKS} tasks  /  {NUM_THREADS} threads  =  {TASKS_PER_THREAD} per thread")
print(f"  Spinning: Thread 3 joins after 800 tasks")
print(f"  Sleeping: All 4 threads start together via cv_work.notify_all()")
print(f"  Run: ./main\n")

plt.show()