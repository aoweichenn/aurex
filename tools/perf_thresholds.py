"""Shared stress-threshold calibration for Aurex perf gates."""

from __future__ import annotations

import os
import platform
from dataclasses import asdict, dataclass


PERF_THRESHOLD_PROFILE_ENV = "AUREX_PERF_THRESHOLD_PROFILE"
PERF_THRESHOLD_SCALE_ENV = "AUREX_PERF_THRESHOLD_SCALE"
PERF_THRESHOLD_DEFAULT_PROFILE = "local"
PERF_THRESHOLD_DEFAULT_SCALE = 1.0
STRESS_ENABLE_LTO_ENV = "AUREX_STRESS_ENABLE_LTO"


@dataclass(frozen=True)
class ThresholdCalibration:
    profile: str
    scale: float
    machine: dict[str, str]

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
