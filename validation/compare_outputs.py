#!/usr/bin/env python3

import sys
import os
import re
import math
import json
import argparse
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Optional, Tuple


@dataclass
class NumericEntry:
    label:    str
    value:    float
    raw_line: str
    line_no:  int


@dataclass
class CompareResult:
    label:       str
    x86_val:     float
    riscv_val:   float
    abs_err:     float
    rel_err:     float
    ulp_dist:    int
    epsilon:     float
    passed:      bool
    note:        str = ""


@dataclass
class SuiteReport:
    x86_file:    str
    riscv_file:  str
    epsilon:     float
    results:     List[CompareResult] = field(default_factory=list)
    total:       int = 0
    passed:      int = 0
    failed:      int = 0


def double_bits(v: float) -> int:
    import struct
    return struct.unpack('Q', struct.pack('d', v))[0]


def ulp_distance(a: float, b: float) -> int:
    if math.isnan(a) or math.isnan(b):
        return 2**63
    if math.isinf(a) and math.isinf(b) and (a > 0) == (b > 0):
        return 0
    ba = double_bits(a)
    bb = double_bits(b)
    sign_a = ba >> 63
    sign_b = bb >> 63
    if sign_a != sign_b:
        return 2**63 if a != b else 0
    return abs(int(ba) - int(bb))


def abs_err(a: float, b: float) -> float:
    return abs(a - b)


def rel_err(a: float, b: float) -> float:
    if b == 0.0:
        return 0.0 if a == 0.0 else float('inf')
    return abs(a - b) / abs(b)


NUMERIC_PATTERNS = [
    re.compile(
        r'(?P<label>[\w\[\]_\-\.]+)\s*[=:]\s*(?P<value>[+-]?\d+\.?\d*(?:[eE][+-]?\d+)?)',
        re.IGNORECASE
    ),
    re.compile(
        r'(?P<label>[\w\[\]_\-\.]+)\s+(?P<value>[+-]?\d+\.\d+(?:[eE][+-]?\d+)?)'
    ),
    re.compile(
        r'err=(?P<value>[+-]?\d+\.?\d*(?:[eE][+-]?\d+)?)'
    ),
    re.compile(
        r'norm\s*=\s*(?P<value>[+-]?\d+\.?\d*(?:[eE][+-]?\d+)?)',
        re.IGNORECASE
    ),
]

SKIP_PATTERNS = [
    re.compile(r'^\s*#'),
    re.compile(r'version', re.IGNORECASE),
    re.compile(r'STATUS', re.IGNORECASE),
    re.compile(r'^\s*$'),
    re.compile(r'PASS|FAIL|OK', re.IGNORECASE),
    re.compile(r'arch=', re.IGNORECASE),
    re.compile(r'target:', re.IGNORECASE),
    re.compile(r'precision:', re.IGNORECASE),
]


def should_skip(line: str) -> bool:
    return any(p.search(line) for p in SKIP_PATTERNS)


def extract_numerics(path: str) -> List[NumericEntry]:
    entries: List[NumericEntry] = []
    with open(path, 'r', errors='replace') as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if should_skip(line):
                continue
            for pat in NUMERIC_PATTERNS:
                for m in pat.finditer(line):
                    try:
                        val = float(m.group('value'))
                    except (ValueError, IndexError):
                        continue
                    if math.isnan(val) or math.isinf(val):
                        continue
                    label_group = 'label' if 'label' in pat.groupindex else None
                    label = m.group(label_group) if label_group else f"line{lineno}"
                    entries.append(NumericEntry(
                        label=label,
                        value=val,
                        raw_line=line,
                        line_no=lineno,
                    ))
    return entries


def pair_entries(
    x86_entries: List[NumericEntry],
    riscv_entries: List[NumericEntry],
) -> List[Tuple[NumericEntry, NumericEntry]]:
    pairs = []
    n = min(len(x86_entries), len(riscv_entries))
    for i in range(n):
        pairs.append((x86_entries[i], riscv_entries[i]))
    return pairs


def compare_files(
    x86_path:   str,
    riscv_path: str,
    epsilon:    float,
    ulp_limit:  int = 4,
) -> SuiteReport:
    report = SuiteReport(
        x86_file=x86_path,
        riscv_file=riscv_path,
        epsilon=epsilon,
    )

    x86_entries   = extract_numerics(x86_path)
    riscv_entries = extract_numerics(riscv_path)

    pairs = pair_entries(x86_entries, riscv_entries)

    for x86e, rve in pairs:
        ae = abs_err(x86e.value, rve.value)
        re_ = rel_err(x86e.value, rve.value)
        ud  = ulp_distance(x86e.value, rve.value)

        passed = (ae <= epsilon) or (re_ <= 1e-10) or (ud <= ulp_limit)

        label = x86e.label if x86e.label == rve.label else f"{x86e.label}/{rve.label}"

        cr = CompareResult(
            label=label,
            x86_val=x86e.value,
            riscv_val=rve.value,
            abs_err=ae,
            rel_err=re_,
            ulp_dist=ud,
            epsilon=epsilon,
            passed=passed,
            note=x86e.raw_line[:80],
        )
        report.results.append(cr)
        report.total += 1
        if passed:
            report.passed += 1
        else:
            report.failed += 1

    return report


def format_markdown(report: SuiteReport) -> str:
    lines = []
    lines.append("# RVForge Numerical Validation Report")
    lines.append("")
    lines.append(f"- **x86_64 output:** `{report.x86_file}`")
    lines.append(f"- **riscv64 output:** `{report.riscv_file}`")
    lines.append(f"- **Epsilon:** `{report.epsilon:.2e}`")
    lines.append(f"- **Total comparisons:** {report.total}")
    lines.append(f"- **Passed:** {report.passed}")
    lines.append(f"- **Failed:** {report.failed}")
    lines.append("")

    if report.failed == 0:
        lines.append("## ✅ All comparisons within epsilon")
    else:
        lines.append(f"## ❌ {report.failed} comparison(s) exceeded epsilon")

    lines.append("")
    lines.append("## Comparison Table")
    lines.append("")
    lines.append("| Label | x86_64 | riscv64 | abs_err | rel_err | ULPs | Result |")
    lines.append("|-------|--------|---------|---------|---------|------|--------|")

    for r in report.results:
        status = "✅" if r.passed else "❌"
        lines.append(
            f"| `{r.label[:30]}` "
            f"| `{r.x86_val:.10e}` "
            f"| `{r.riscv_val:.10e}` "
            f"| `{r.abs_err:.3e}` "
            f"| `{r.rel_err:.3e}` "
            f"| {r.ulp_dist} "
            f"| {status} |"
        )

    if report.failed > 0:
        lines.append("")
        lines.append("## Failed Comparisons Detail")
        lines.append("")
        for r in report.results:
            if not r.passed:
                lines.append(f"### `{r.label}`")
                lines.append(f"- x86_64:  `{r.x86_val:.15e}`")
                lines.append(f"- riscv64: `{r.riscv_val:.15e}`")
                lines.append(f"- abs_err: `{r.abs_err:.6e}` (epsilon={r.epsilon:.2e})")
                lines.append(f"- rel_err: `{r.rel_err:.6e}`")
                lines.append(f"- ULPs:    `{r.ulp_dist}`")
                lines.append(f"- Context: `{r.note}`")
                lines.append("")

    return "\n".join(lines)


def format_json(report: SuiteReport) -> str:
    data = {
        "x86_file":   report.x86_file,
        "riscv_file": report.riscv_file,
        "epsilon":    report.epsilon,
        "total":      report.total,
        "passed":     report.passed,
        "failed":     report.failed,
        "results": [
            {
                "label":     r.label,
                "x86_val":   r.x86_val,
                "riscv_val": r.riscv_val,
                "abs_err":   r.abs_err,
                "rel_err":   r.rel_err,
                "ulp_dist":  r.ulp_dist,
                "passed":    r.passed,
            }
            for r in report.results
        ],
    }
    return json.dumps(data, indent=2)


def print_summary(report: SuiteReport) -> None:
    width = 70
    print("=" * width)
    print(f"  RVForge Validation: {os.path.basename(report.riscv_file)}")
    print("=" * width)
    print(f"  Epsilon:    {report.epsilon:.2e}")
    print(f"  Total:      {report.total}")
    print(f"  Passed:     {report.passed}")
    print(f"  Failed:     {report.failed}")
    print("-" * width)

    for r in report.results:
        status = "PASS" if r.passed else "FAIL"
        print(f"  [{status}] {r.label[:35]:<35} "
              f"abs={r.abs_err:.2e}  ulp={r.ulp_dist}")

    print("=" * width)
    overall = "PASS" if report.failed == 0 else "FAIL"
    print(f"  OVERALL: {overall}")
    print("=" * width)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare x86_64 vs riscv64 numerical outputs"
    )
    parser.add_argument("x86_file",   help="x86_64 output file")
    parser.add_argument("riscv_file", help="riscv64 output file")
    parser.add_argument(
        "--epsilon", type=float, default=1e-10,
        help="Absolute epsilon threshold (default: 1e-10)"
    )
    parser.add_argument(
        "--ulp-limit", type=int, default=4,
        help="ULP distance limit (default: 4)"
    )
    parser.add_argument(
        "--output-md", type=str, default=None,
        help="Write Markdown report to this file"
    )
    parser.add_argument(
        "--output-json", type=str, default=None,
        help="Write JSON report to this file"
    )
    parser.add_argument(
        "--strict", action="store_true",
        help="Exit 1 if ANY comparison fails (default: exit 1 only if failed > 0)"
    )
    args = parser.parse_args()

    if not os.path.exists(args.x86_file):
        print(f"ERROR: x86_64 file not found: {args.x86_file}", file=sys.stderr)
        return 2

    if not os.path.exists(args.riscv_file):
        print(f"ERROR: riscv64 file not found: {args.riscv_file}", file=sys.stderr)
        return 2

    report = compare_files(
        x86_path=args.x86_file,
        riscv_path=args.riscv_file,
        epsilon=args.epsilon,
        ulp_limit=args.ulp_limit,
    )

    print_summary(report)

    if args.output_md:
        Path(args.output_md).parent.mkdir(parents=True, exist_ok=True)
        with open(args.output_md, 'w') as f:
            f.write(format_markdown(report))
        print(f"\nMarkdown report: {args.output_md}")

    if args.output_json:
        Path(args.output_json).parent.mkdir(parents=True, exist_ok=True)
        with open(args.output_json, 'w') as f:
            f.write(format_json(report))
        print(f"JSON report:     {args.output_json}")

    if args.strict:
        return 0 if report.failed == 0 else 1
    return 0 if report.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
