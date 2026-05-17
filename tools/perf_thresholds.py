"""Shared stress-threshold calibration for Aurex perf gates."""

from __future__ import annotations

import os
import json
import pathlib
import platform
import re
import subprocess
import time
from dataclasses import asdict, dataclass
from typing import Sequence


PERF_THRESHOLD_PROFILE_ENV = "AUREX_PERF_THRESHOLD_PROFILE"
PERF_THRESHOLD_SCALE_ENV = "AUREX_PERF_THRESHOLD_SCALE"
PERF_THRESHOLD_DEFAULT_PROFILE = "local"
PERF_THRESHOLD_DEFAULT_SCALE = 1.0
STRESS_ENABLE_LTO_ENV = "AUREX_STRESS_ENABLE_LTO"
DARWIN_TIME_RSS_PATTERN = re.compile(
    r"(?:maximum resident set size:\s+(\d+)|(\d+)\s+maximum resident set size)",
    re.IGNORECASE,
)
DARWIN_TIME_CPU_PATTERN = re.compile(
    r"^\s*(?P<real>\d+(?:\.\d+)?)\s+real\s+"
    r"(?P<user>\d+(?:\.\d+)?)\s+user\s+"
    r"(?P<sys>\d+(?:\.\d+)?)\s+sys\s*$",
    re.MULTILINE,
)
GNU_TIME_RSS_PATTERN = re.compile(r"Maximum resident set size \(kbytes\):\s+(\d+)", re.IGNORECASE)
GNU_TIME_ELAPSED_PATTERN = re.compile(
    r"Elapsed \(wall clock\) time \(h:mm:ss or m:ss\):\s+([^\n]+)",
    re.IGNORECASE,
)


@dataclass(frozen=True)
class ThresholdCalibration:
    profile: str
    scale: float
    machine: dict[str, str]

    def to_json(self) -> dict[str, object]:
        return asdict(self)


@dataclass(frozen=True)
class ProcessMetrics:
    elapsed_ms: float
    max_rss_mib: float | None
    user_time_s: float | None
    system_time_s: float | None
    time_tool_elapsed_s: float | None
    minor_page_faults: int | None
    major_page_faults: int | None
    voluntary_context_switches: int | None
    involuntary_context_switches: int | None

    def to_json(self) -> dict[str, object]:
        return asdict(self)


def parse_positive_float(text: str | None, name: str) -> float:
    if text is None or not text.strip():
        return PERF_THRESHOLD_DEFAULT_SCALE
    value = float(text)
    if value <= 0:
        raise ValueError(f"{name} must be positive")
    return value


def machine_info() -> dict[str, str]:
    return {
        "system": platform.system(),
        "release": platform.release(),
        "machine": platform.machine(),
        "processor": platform.processor(),
    }


def make_calibration(profile: str | None = None, scale: str | None = None) -> ThresholdCalibration:
    selected_profile = profile
    if selected_profile is None or not selected_profile.strip():
        selected_profile = os.environ.get(PERF_THRESHOLD_PROFILE_ENV, PERF_THRESHOLD_DEFAULT_PROFILE)
    selected_scale = scale
    if selected_scale is None:
        selected_scale = os.environ.get(PERF_THRESHOLD_SCALE_ENV)
    return ThresholdCalibration(
        profile=selected_profile.strip() or PERF_THRESHOLD_DEFAULT_PROFILE,
        scale=parse_positive_float(selected_scale, "--threshold-scale"),
        machine=machine_info(),
    )


def scaled_threshold(value: float | None, calibration: ThresholdCalibration) -> float | None:
    if value is None:
        return None
    return value * calibration.scale


def parse_bool_env(name: str) -> bool:
    text = os.environ.get(name)
    if text is None or not text.strip():
        return False
    normalized = text.strip().lower()
    if normalized in {"1", "true", "yes", "on"}:
        return True
    if normalized in {"0", "false", "no", "off"}:
        return False
    raise ValueError(f"{name} must be one of: 1, true, yes, on, 0, false, no, off")


def stress_lto_enabled() -> bool:
    return parse_bool_env(STRESS_ENABLE_LTO_ENV)


def stress_build_options() -> dict[str, object]:
    return {
        "build_type": "Release",
        "lto": stress_lto_enabled(),
    }


def _parse_int(text: str) -> int | None:
    try:
        return int(text.strip())
    except ValueError:
        return None


def _parse_float(text: str) -> float | None:
    try:
        return float(text.strip())
    except ValueError:
        return None


def _parse_darwin_int_field(stderr: str, label: str) -> int | None:
    pattern = re.compile(rf"^\s*(\d+)\s+{re.escape(label)}\s*$", re.IGNORECASE | re.MULTILINE)
    match = pattern.search(stderr)
    return None if match is None else _parse_int(match.group(1))


def _parse_gnu_field(stderr: str, label: str) -> str | None:
    pattern = re.compile(rf"^\s*{re.escape(label)}:\s*(.+?)\s*$", re.IGNORECASE | re.MULTILINE)
    match = pattern.search(stderr)
    return None if match is None else match.group(1)


def _parse_gnu_elapsed_seconds(text: str | None) -> float | None:
    if text is None:
        return None
    stripped = text.strip()
    if not stripped:
        return None
    parts = stripped.split(":")
    try:
        if len(parts) == 1:
            return float(parts[0])
        if len(parts) == 2:
            minutes = int(parts[0])
            seconds = float(parts[1])
            return minutes * 60.0 + seconds
        if len(parts) == 3:
            hours = int(parts[0])
            minutes = int(parts[1])
            seconds = float(parts[2])
            return hours * 3600.0 + minutes * 60.0 + seconds
    except ValueError:
        return None
    return None


def parse_process_metrics(stderr: str, elapsed_ms: float) -> ProcessMetrics:
    if platform.system() == "Darwin":
        rss_match = DARWIN_TIME_RSS_PATTERN.search(stderr)
        max_rss_mib = None
        if rss_match is not None:
            rss_bytes = int(next(group for group in rss_match.groups() if group is not None))
            max_rss_mib = rss_bytes / (1024.0 * 1024.0)
        cpu_match = DARWIN_TIME_CPU_PATTERN.search(stderr)
        user_time_s = None if cpu_match is None else _parse_float(cpu_match.group("user"))
        system_time_s = None if cpu_match is None else _parse_float(cpu_match.group("sys"))
        real_time_s = None if cpu_match is None else _parse_float(cpu_match.group("real"))
        return ProcessMetrics(
            elapsed_ms=elapsed_ms,
            max_rss_mib=max_rss_mib,
            user_time_s=user_time_s,
            system_time_s=system_time_s,
            time_tool_elapsed_s=real_time_s,
            minor_page_faults=_parse_darwin_int_field(stderr, "page reclaims"),
            major_page_faults=_parse_darwin_int_field(stderr, "page faults"),
            voluntary_context_switches=_parse_darwin_int_field(stderr, "voluntary context switches"),
            involuntary_context_switches=_parse_darwin_int_field(stderr, "involuntary context switches"),
        )

    rss_match = GNU_TIME_RSS_PATTERN.search(stderr)
    max_rss_mib = None if rss_match is None else int(rss_match.group(1)) / 1024.0
    return ProcessMetrics(
        elapsed_ms=elapsed_ms,
        max_rss_mib=max_rss_mib,
        user_time_s=_parse_float(_parse_gnu_field(stderr, "User time (seconds)") or ""),
        system_time_s=_parse_float(_parse_gnu_field(stderr, "System time (seconds)") or ""),
        time_tool_elapsed_s=_parse_gnu_elapsed_seconds(
            _parse_gnu_field(stderr, "Elapsed (wall clock) time (h:mm:ss or m:ss)")
        ),
        minor_page_faults=_parse_int(_parse_gnu_field(stderr, "Minor (reclaiming a frame) page faults") or ""),
        major_page_faults=_parse_int(_parse_gnu_field(stderr, "Major (requiring I/O) page faults") or ""),
        voluntary_context_switches=_parse_int(_parse_gnu_field(stderr, "Voluntary context switches") or ""),
        involuntary_context_switches=_parse_int(_parse_gnu_field(stderr, "Involuntary context switches") or ""),
    )


def run_timed_command(
    base_cmd: Sequence[str],
    cwd: pathlib.Path,
) -> tuple[subprocess.CompletedProcess[str], ProcessMetrics]:
    time_tool = pathlib.Path("/usr/bin/time")
    if time_tool.exists() and os.access(time_tool, os.X_OK):
        if platform.system() == "Darwin":
            cmd = [str(time_tool), "-l", *base_cmd]
        else:
            cmd = [str(time_tool), "-v", *base_cmd]
    else:
        cmd = list(base_cmd)

    started = time.perf_counter()
    completed = subprocess.run(
        cmd,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    elapsed_ms = (time.perf_counter() - started) * 1000.0
    return completed, parse_process_metrics(completed.stderr, elapsed_ms)


def load_compiler_profile(path: pathlib.Path) -> dict[str, object] | None:
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def format_optional_float(value: float | None, precision: int = 3) -> str:
    if value is None:
        return "n/a"
    return f"{value:.{precision}f}"


def format_optional_int(value: int | None) -> str:
    return "n/a" if value is None else str(value)


def compiler_profile_phases(profile: dict[str, object] | None) -> list[dict[str, object]]:
    if profile is None:
        return []
    phases = profile.get("phases")
    if not isinstance(phases, list):
        return []
    return [phase for phase in phases if isinstance(phase, dict)]


def print_compiler_phase_report(
    label: str,
    profile_path: str,
    profile: dict[str, object] | None,
) -> None:
    phases = compiler_profile_phases(profile)
    print()
    print(f"Compiler phases for {label}: {profile_path}")
    if not phases:
        print("  no compiler profile phases recorded")
        return
    print(
        f"  {'phase':<28} {'elapsed_ms':>12} {'rss_after_MiB':>14} "
        f"{'rss_delta_MiB':>14} {'detail'}"
    )
    print(
        f"  {'-' * 28} {'-' * 12} {'-' * 14} "
        f"{'-' * 14} {'-' * 24}"
    )
    for phase in phases:
        name = str(phase.get("name", ""))
        detail = str(phase.get("detail", ""))
        elapsed = phase.get("elapsed_ms")
        rss_after = phase.get("rss_mib_after")
        rss_delta = phase.get("rss_delta_mib")
        elapsed_text = format_optional_float(elapsed if isinstance(elapsed, (float, int)) else None)
        rss_after_text = format_optional_float(rss_after if isinstance(rss_after, (float, int)) else None, 1)
        rss_delta_text = format_optional_float(rss_delta if isinstance(rss_delta, (float, int)) else None, 1)
        print(
            f"  {name:<28} {elapsed_text:>12} {rss_after_text:>14} "
            f"{rss_delta_text:>14} {detail}"
        )
