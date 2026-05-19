#!/usr/bin/env python3
"""Regenerate the benchmark plot gallery in README.md."""

from __future__ import annotations

import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
BENCHMARKS_DIR = REPO_ROOT / "benchmarks"
README = REPO_ROOT / "README.md"
PLOTS_DIR = BENCHMARKS_DIR / "plots"

START = "<!-- BENCHMARK_RESULTS_START -->"
END = "<!-- BENCHMARK_RESULTS_END -->"

PLOT_EXTENSIONS = (".png", ".svg", ".pdf")
MAX_PLOTS_PER_BENCHMARK = 3


def _load_benchmarks():
    sys.path.insert(0, str(BENCHMARKS_DIR))
    from benchmarks.registry import ALL_BENCHMARKS

    return ALL_BENCHMARKS


def _natural_key(path: Path):
    return [
        int(part) if part.isdigit() else part.lower()
        for part in re.split(r"(\d+)", path.stem)
    ]


def _plot_paths(name: str) -> list[Path]:
    paths: list[Path] = []
    for ext in PLOT_EXTENSIONS:
        exact = PLOTS_DIR / f"{name}{ext}"
        if exact.exists():
            paths.append(exact)
        paths.extend(PLOTS_DIR.glob(f"{name}_*{ext}"))
    return sorted(set(paths), key=_natural_key)


def _representative_paths(paths: list[Path]) -> list[Path]:
    if len(paths) <= MAX_PLOTS_PER_BENCHMARK:
        return paths
    indexes = {
        0,
        len(paths) // 2,
        len(paths) - 1,
    }
    return [paths[i] for i in sorted(indexes)]


def _escape_cell(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", " ")


def _plot_cell(name: str) -> str:
    all_paths = _plot_paths(name)
    paths = _representative_paths(all_paths)
    if not paths:
        return "_pending_"

    images = []
    for path in paths:
        rel = path.relative_to(REPO_ROOT).as_posix()
        label = path.stem.removeprefix(f"{name}_")
        if path.stem == name:
            label = "overview"
        else:
            label = label.replace("_", " ")
        images.append(f'<img src="{rel}" alt="{path.stem}" width="320"><br><sub>{label}</sub>')
    if len(all_paths) > len(paths):
        images.append(f"<sub>{len(all_paths) - len(paths)} more plot(s) omitted</sub>")
    return "<br><br>".join(images)


def _build_section() -> str:
    benchmarks = _load_benchmarks()
    lines = [
        "## Benchmark Results",
        "",
        START,
        "",
        "| Benchmark | Description | Plot |",
        "| --- | --- | --- |",
    ]
    for name in sorted(benchmarks):
        benchmark = benchmarks[name]
        lines.append(
            f"| `{name}` | {_escape_cell(benchmark.description)} | {_plot_cell(name)} |"
        )
    lines.extend(["", END])
    return "\n".join(lines)


def _replace_or_append(readme: str, section: str) -> str:
    pattern = re.compile(
        rf"## Benchmark Results\s*\n\s*{re.escape(START)}.*?{re.escape(END)}\s*",
        re.DOTALL,
    )
    if pattern.search(readme):
        return pattern.sub(section + "\n", readme)
    return readme.rstrip() + "\n\n" + section + "\n"


def main() -> int:
    readme = README.read_text()
    README.write_text(_replace_or_append(readme, _build_section()))
    print(f"Updated {README.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
