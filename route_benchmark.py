import json
from pathlib import Path
import subprocess
import sys


ALGORITHMS = {"dijkstra": "Дейкстра", "bellman-ford": "Беллман–Форд"}
CSV_FIELDS = ("topology", "vertices", "source", "seed", "repeats",
              "algorithm", "median_us", "peak_rss_bytes")


def compare_large_graph(program, topology, size, seed, repeats):
    rows = []
    distances = None
    for algorithm in ALGORITHMS:
        request = f"large-benchmark {algorithm} {topology} {size} {seed} {repeats}\n"
        result = subprocess.run(
            [sys.executable, str(Path(__file__).resolve())],
            input=json.dumps({"program": str(program), "request": request}),
            capture_output=True, encoding="utf-8", timeout=125,
        )
        if result.returncode:
            raise ValueError(result.stderr.strip() or "Не удалось измерить время и память.")
        data = json.loads(result.stdout)
        if distances is None:
            distances = data["distances"]
        elif distances != data["distances"]:
            raise ValueError("Результаты поиска разошлись.")
        rows.append({
            "topology": topology, "vertices": size, "source": size - 1,
            "seed": seed, "repeats": repeats, "algorithm": algorithm,
            "median_us": data["median_us"], "peak_rss_bytes": data["peak_rss_bytes"],
        })
    return rows


def measure_child():
    # У процесса-измерителя один дочерний процесс, поэтому RSS относится к одному алгоритму.
    if sys.platform != "darwin" and not sys.platform.startswith("linux"):
        raise ValueError("Замер памяти доступен на macOS и Linux.")
    import resource

    data = json.load(sys.stdin)
    result = subprocess.run(
        [data["program"]], input=data["request"], capture_output=True,
        encoding="utf-8", timeout=120,
    )
    if result.returncode:
        raise ValueError(result.stderr.strip() or "Ошибка запуска C++-адаптера.")
    output = json.loads(result.stdout)
    peak = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    output["peak_rss_bytes"] = peak if sys.platform == "darwin" else peak * 1024
    json.dump(output, sys.stdout)


if __name__ == "__main__":
    try:
        measure_child()
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from error
