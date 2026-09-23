import math
from collections.abc import Callable
from dataclasses import dataclass
from typing import Literal


Mapping = Literal["linear", "logarithmic", "power"]
Formatter = Callable[[float], str]


def clamp(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(maximum, value))


def linear_from_normalized(
    normalized: float, minimum: float, maximum: float
) -> float:
    normalized = clamp(normalized, 0.0, 1.0)
    return minimum + (maximum - minimum) * normalized


def linear_to_normalized(value: float, minimum: float, maximum: float) -> float:
    value = clamp(value, minimum, maximum)
    return (value - minimum) / (maximum - minimum)


def log_from_normalized(
    normalized: float, minimum: float, maximum: float
) -> float:
    normalized = clamp(normalized, 0.0, 1.0)
    return minimum * ((maximum / minimum) ** normalized)


def log_to_normalized(value: float, minimum: float, maximum: float) -> float:
    value = clamp(value, minimum, maximum)
    return math.log(value / minimum) / math.log(maximum / minimum)


def power_from_normalized(
    normalized: float,
    minimum: float,
    maximum: float,
    exponent: float,
) -> float:
    normalized = clamp(normalized, 0.0, 1.0)
    return minimum + (maximum - minimum) * (normalized**exponent)


def power_to_normalized(
    value: float,
    minimum: float,
    maximum: float,
    exponent: float,
) -> float:
    value = clamp(value, minimum, maximum)
    ratio = (value - minimum) / (maximum - minimum)
    if ratio == 0.0:
        return 0.0
    return ratio ** (1.0 / exponent)


def format_frequency(value: float) -> str:
    if value >= 1000.0:
        return f"{value / 1000.0:.1f} kHz"
    return f"{value:.0f} Hz"


@dataclass(frozen=True)
class ControlSpec:
    minimum: float
    maximum: float
    default: float
    formatter: Formatter
    mapping: Mapping = "linear"
    exponent: float = 1.0

    def from_normalized(self, normalized: float) -> float:
        if self.mapping == "logarithmic":
            return log_from_normalized(normalized, self.minimum, self.maximum)
        if self.mapping == "power":
            return power_from_normalized(
                normalized, self.minimum, self.maximum, self.exponent
            )
        return linear_from_normalized(normalized, self.minimum, self.maximum)

    def to_normalized(self, value: float) -> float:
        if self.mapping == "logarithmic":
            return log_to_normalized(value, self.minimum, self.maximum)
        if self.mapping == "power":
            return power_to_normalized(
                value, self.minimum, self.maximum, self.exponent
            )
        return linear_to_normalized(value, self.minimum, self.maximum)


# These are performance ranges for the desktop UI. The firmware retains its
# wider absolute protocol/DSP ranges and receives the mapped physical values.
CONTROL_SPECS = {
    "TUNE_HZ": ControlSpec(80.0, 1600.0, 220.0, format_frequency, "logarithmic"),
    "LFO_RATE_HZ": ControlSpec(
        0.15, 8.0, 0.70, lambda value: f"{value:.2f} Hz", "logarithmic"
    ),
    "LFO_DEPTH_OCT": ControlSpec(
        0.0, 1.25, 1.0, lambda value: f"{value:.2f} oct"
    ),
    "DECAY_MS": ControlSpec(
        0.0, 1500.0, 120.0, lambda value: f"{value:.0f} ms", "power", 2.0
    ),
    "DELAY_MS": ControlSpec(
        50.0, 1000.0, 360.0, lambda value: f"{value:.0f} ms"
    ),
    "FEEDBACK": ControlSpec(
        0.0, 1.05, 0.62, lambda value: f"{value * 100:.0f} %", "power", 0.8
    ),
    "ECHO_LEVEL": ControlSpec(
        0.0, 1.0, 0.45, lambda value: f"{value * 100:.0f} %"
    ),
    "HPF_HZ": ControlSpec(50.0, 4000.0, 60.0, format_frequency, "logarithmic"),
    "LPF_HZ": ControlSpec(
        400.0, 16000.0, 7000.0, format_frequency, "logarithmic"
    ),
    "MASTER": ControlSpec(
        0.0, 1.0, 0.50, lambda value: f"{value * 100:.0f} %"
    ),
}
