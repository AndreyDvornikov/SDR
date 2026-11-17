"""Показывает I/Q-составляющие и созвездия txdata.pcm и rxdata.pcm."""

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
    axis.plot(
        samples, decimate(i_component), color="royalblue", linewidth=0.7, label="I"
    )
    axis.plot(samples, decimate(q_component), color="crimson", linewidth=0.7, label="Q")
    axis.set_title(path.name)
    axis.set_xlabel("sample")
    axis.set_ylabel("amplitude")
    axis.grid(alpha=0.3)
    axis.legend()


def add_scatter(axis, path: Path) -> None:
    i_component, q_component = read_pcm(path)
    i_points = decimate(i_component)
    q_points = decimate(q_component)
    axis.scatter(
        i_points,
        q_points,
        color="crimson",
        s=5,
        alpha=0.5,
    )
    # Symmetric limits keep (0, 0) at the center. A percentile avoids a few
    # timestamp/marker samples in the TX buffer stretching the whole plot.
    extent = float(np.percentile(np.abs(np.concatenate((i_points, q_points))), 99.5))
    extent = max(extent, 1.0)
    axis.set_xlim(-extent, extent)
    axis.set_ylim(-extent, extent)
    axis.set_title(f"{path.name}: constellation")
    axis.set_xlabel("I")
    axis.set_ylabel("Q")
    axis.set_aspect("equal", adjustable="box")
    axis.grid(alpha=0.3)


def main() -> None:
    files = [Path("../pcms/txdata.pcm"), Path("../pcms/rxdata.pcm")]
    missing = [str(path) for path in files if not path.exists()]
    if missing:
        raise SystemExit(f"Не найдены файлы: {', '.join(missing)}")

    _, axes = plt.subplots(2, 2, layout="constrained", figsize=(14, 9))
    for row, path in enumerate(files):
        add_plot(axes[row, 0], path)
        add_scatter(axes[row, 1], path)
    plt.show()


if __name__ == "__main__":
    main()
