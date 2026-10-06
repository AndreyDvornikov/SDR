"""Показывает I/Q-составляющие записей txdata.pcm и rxdata.pcm."""

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

MAX_POINTS = 50_000


def read_pcm(path: Path) -> tuple[np.ndarray, np.ndarray]:
    """Читает CS16 как пары I, Q и возвращает две компоненты."""
    data = np.fromfile(path, dtype=np.int16)
    data = data[: len(data) // 2 * 2]
    return data[0::2].astype(np.int32), data[1::2].astype(np.int32)


def decimate(values: np.ndarray) -> np.ndarray:
    """Оставляет достаточно точек, чтобы график не тормозил."""
    step = max(1, len(values) // MAX_POINTS + 1)
    return values[::step]


def add_plot(axis, path: Path) -> None:
    i_component, q_component = read_pcm(path)
    samples = np.arange(len(i_component))[:: max(1, len(i_component) // MAX_POINTS + 1)]
    axis.plot(samples, decimate(i_component), color="royalblue", linewidth=0.7, label="I")
    axis.plot(samples, decimate(q_component), color="crimson", linewidth=0.7, label="Q")
    axis.set_title(path.name)
    axis.set_xlabel("sample")
    axis.set_ylabel("amplitude")
    axis.grid(alpha=0.3)
    axis.legend()


def main() -> None:
    files = [Path("txdata.pcm"), Path("rxdata.pcm")]
    missing = [str(path) for path in files if not path.exists()]
    if missing:
        raise SystemExit(f"Не найдены файлы: {', '.join(missing)}")

    _, axes = plt.subplots(2, 1, layout="constrained", figsize=(12, 7))
    for axis, path in zip(axes, files):
        add_plot(axis, path)
    plt.show()


if __name__ == "__main__":
    main()
