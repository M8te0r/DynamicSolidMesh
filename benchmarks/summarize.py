#!/usr/bin/env python3
"""Aggregate benchmark CSV files using only the Python standard library."""
import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path

TYPES = ("Vertex", "Dedge", "Halfface", "Cell")
OPERATIONS = {
    "append": "从空容器批量插入",
    "get_sequential_live": "顺序 get（全部有效）",
    "get_random_live": "随机 get（全部有效）",
    "contains_then_get_random_live": "随机 contains 后 get（全部有效）",
    "get_random_half_stale": "随机 get（50% 失效）",
    "contains_random_half_stale": "随机 contains（50% 失效）",
    "iterate_keys_dense": "遍历 key 后 get（100% 存活）",
    "iterate_native_dense": "原生接口遍历（100% 存活）",
    "iterate_keys_50pct_live": "遍历 key 后 get（50% 存活）",
    "iterate_native_50pct_live": "原生接口遍历（50% 存活）",
    "iterate_keys_10pct_live": "遍历 key 后 get（10% 存活）",
    "iterate_native_10pct_live": "原生接口遍历（10% 存活）",
    "erase_random_half": "随机删除半数元素",
    "refill_deleted_half": "回填刚删除的半数元素",
    "erase_insert_pair": "随机删一个、插一个",
    "copy_dense": "复制无空洞容器",
    "destroy_dense_copy": "析构完整副本",
    "destroy_10pct_live": "析构仅剩一成的容器",
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    root = args.directory
    with (root / "timing.csv").open() as stream:
        rows = list(csv.DictReader(stream))
    groups = defaultdict(list)
    trials = defaultdict(set)
    skipped = 0
    for row in rows:
        if row["status"] != "ok":
            skipped += 1
            continue
        key = (row["type"], int(row["n"]), row["operation"], row["backend"])
        groups[key].append(float(row["ns_per_item"]))
        trials[key].add(int(row["trial"]))
    counts = {len(samples) for samples in groups.values()}
    if len(counts) != 1 or any(len(trials[key]) != len(samples) for key, samples in groups.items()):
        raise RuntimeError("Inconsistent or duplicate samples")
    count = counts.pop()
    if count < 4:
        raise RuntimeError("At least four measured repetitions are required")

    fields = ["type", "n", "operation", "backend", "samples", "median_ns", "q1_ns", "q3_ns", "min_ns", "max_ns"]
    medians = {}
    with (root / "statistics.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(fields)
        for key, samples in sorted(groups.items()):
            q1, _, q3 = statistics.quantiles(samples, n=4, method="inclusive")
            median = statistics.median(samples)
            medians[key] = median
            writer.writerow([*key, len(samples), median, q1, q3, min(samples), max(samples)])

    def value(typ, n, operation, backend):
        return medians[(typ, n, operation, backend)]

    def pair(typ, n, operation, divisor=1):
        a, b = (value(typ, n, operation, backend) / divisor for backend in ("custom", "dod"))
        return f"{a:.3g} / {b:.3g}"

    lines = [
        "# Slot map 实测数据汇总",
        "",
        f"每个有效测试点取 {count} 轮中位数，另有 1 轮预热不计入。共 {len(rows)} 行原始记录，"
        f"其中 {skipped} 行因缺少正确的 dod 稀疏复制对照而跳过。",
        "",
        "每格是 **自定义 / dod**，均为实测绝对耗时。越小越快。",
        "操作单位为 ns；批量操作按每元素计，查询按每次计，遍历按每个存活元素计，"
        "删插按每一对计。稀疏遍历仍会扫描空槽，因此不可把 ns/存活元素当作 ns/槽位。",
        "",
        "完整四分位区间和最小/最大值见 statistics.csv；方法、限制及复现方式见 ../../README.md。",
    ]
    sizes = sorted({n for _, n, op, _ in groups if op == "append"})
    for n in sizes:
        lines += ["", f"## 初始元素数 {n:,}", "", "| 操作（ns/项） | " + " | ".join(TYPES) + " |",
                  "|---|---:|---:|---:|---:|"]
        for operation, label in OPERATIONS.items():
            lines.append("| " + label + " | " + " | ".join(pair(typ, n, operation) for typ in TYPES) + " |")
        lines += ["", "| 操作 | " + " | ".join(TYPES) + " |", "|---|---:|---:|---:|---:|",
                  "| 含删除槽位的复制 | 双方跳过 | 双方跳过 | 双方跳过 | 双方跳过 |"]

    growth_sizes = sorted({n for _, n, op, _ in groups if op == "append_at_capacity_boundary"})
    lines += ["", "## 容量边界插入", "", "每格为自定义 / dod，单位 **µs/单次插入**，包含读取新元素校验值。",
              "", "| 已有元素数 | " + " | ".join(TYPES) + " |", "|---|---:|---:|---:|---:|"]
    for n in growth_sizes:
        lines.append(f"| {n:,} | " + " | ".join(pair(typ, n, "append_at_capacity_boundary", 1000) for typ in TYPES) + " |")

    with (root / "memory.csv").open() as stream:
        memory = {(r["type"], int(r["n"]), r["backend"], r["stage"]): r for r in csv.DictReader(stream)}
    lines += ["", "## 堆空间", "", "单位 MiB（2²⁰ 字节），统计容器及元素成员请求的字节数。"
              "排除外部 key 数组、测试数据、分配器取整和统计记录头；不是进程 RSS。"
              "峰值捕获分配/释放过程中同时存活的堆空间，包含 vector 增长时的新旧数组。"]
    for n in sizes:
        lines += ["", f"### {n:,} 个初始元素", "",
                  "| 类型 | 密集时 自定义 / dod | 构建峰值 自定义 / dod | 仅剩一成 自定义 / dod | 全部删除后 自定义 / dod |",
                  "|---|---:|---:|---:|---:|"]
        for typ in TYPES:
            cells = []
            for stage, field in [("dense", "live_requested_bytes"), ("dense", "phase_peak_requested_bytes"),
                                 ("10pct_live", "live_requested_bytes"), ("all_erased", "live_requested_bytes")]:
                cells.append(" / ".join(f"{int(memory[(typ, n, b, stage)][field]) / 2**20:.2f}" for b in ("custom", "dod")))
            lines.append("| " + typ + " | " + " | ".join(cells) + " |")
    (root / "SUMMARY.md").write_text("\n".join(lines) + "\n")
    print(f"Validated {len(groups)} measured groups x {count} samples; wrote statistics.csv and SUMMARY.md")


if __name__ == "__main__":
    main()
