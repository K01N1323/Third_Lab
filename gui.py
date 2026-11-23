from concurrent.futures import ThreadPoolExecutor
import csv
import json
import math
from pathlib import Path
import subprocess
import tempfile
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from route_benchmark import ALGORITHMS as BENCHMARK_ALGORITHMS
from route_benchmark import CSV_FIELDS, compare_large_graph


PROJECT = Path(__file__).resolve().parent

BACKGROUND = "#202729"
PANEL = "#182022"
TEXT = "#EAF1EE"
MUTED = "#B5C4BF"
ACCENT = "#91D8C5"
BORDER = "#455A53"
GROUP_COLORS = (ACCENT, "#F0C583", "#A9C5F8", "#E2ADE5", "#F4AB9F", "#BBD98A")

ALGORITHMS = {"Дейкстра": "dijkstra", "Беллман–Форд": "bellman-ford"}
TOPOLOGIES = {"Цепочка": "chain", "Кольцо": "cycle", "Отдельные цепочки": "separate"}


def run_core(program, *args, graph=None):
    # Собираю адаптер один раз за запуск окна.
    if not program.is_file():
        sources = ("Src/gui_adapter.cpp", "Ui/GraphAdapter.cpp", "Ui/GraphIO.cpp")
        build = subprocess.run(
            ["c++", "-std=c++17", "-O2", "-I", str(PROJECT),
             *(str(PROJECT / source) for source in sources), "-o", str(program)],
            capture_output=True, encoding="utf-8", timeout=90,
        )
        if build.returncode:
            raise ValueError(build.stderr.strip() or "Не удалось собрать C++-адаптер.")
    request = " ".join(str(arg) for arg in args) + "\n"
    result = subprocess.run(
        [str(program)], input=request + (graph or ""), capture_output=True,
        encoding="utf-8", timeout=60,
    )
    if result.returncode:
        raise ValueError(result.stderr.strip() or "Ошибка при работе с графом.")
    return result.stdout


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Маршруты по кампусу")
        self.geometry("1080x760")
        self.minsize(880, 635)
        self.configure_styles()

        self.vertices = []
        self.edges = []
        self.graph_text = ""
        self.path = []
        self.groups = []
        self.last_timings = None
        self.source = tk.StringVar()
        self.destination = tk.StringVar()
        self.algorithm = tk.StringVar(value="Дейкстра")
        self.topology = tk.StringVar(value="Кольцо")
        self.size = tk.StringVar(value="8")
        self.seed = tk.StringVar(value="42")
        self.group_count = tk.StringVar(value="2")
        self.repeats = tk.StringVar(value="7")
        self.status = tk.StringVar()
        self.summary = tk.StringVar(value="Схема кампуса")

        self.folder = tempfile.TemporaryDirectory(prefix="campus-ui-")
        self.program = Path(self.folder.name) / "graph_adapter"
        self.worker = ThreadPoolExecutor(max_workers=1)
        self.comparison = None
        self.benchmark_window = None
        self.protocol("WM_DELETE_WINDOW", self.close)
        self.build_window()
        self.update_buttons()
        self.load_example()

    def configure_styles(self):
        self.configure(background=BACKGROUND)
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure(".", font=("Helvetica", 12), background=BACKGROUND, foreground=TEXT)
        style.configure("TFrame", background=BACKGROUND)
        style.configure("TLabel", background=BACKGROUND, foreground=TEXT)
        style.configure("Title.TLabel", font=("Helvetica", 24, "bold"))
        style.configure("Section.TLabel", font=("Helvetica", 13, "bold"))
        style.configure("Muted.TLabel", font=("Helvetica", 11), foreground=MUTED)
        style.configure("Error.TLabel", font=("Helvetica", 11), foreground="#F4AB9F")
        style.configure("TButton", background="#35433F", foreground=TEXT,
                        borderwidth=0, padding=(12, 8))
        style.map("TButton", background=[("active", "#455A53")],
                  foreground=[("disabled", "#899C95")])
        style.configure("Accent.TButton", background=ACCENT, foreground="#152A24",
                        font=("Helvetica", 12, "bold"))
        style.map("Accent.TButton", background=[("disabled", "#35433F"), ("active", "#A8E3D4")],
                  foreground=[("disabled", "#899C95"), ("!disabled", "#152A24")])
        for widget in ("TEntry", "TCombobox"):
            style.configure(widget, fieldbackground=PANEL, foreground=TEXT,
                            background="#35433F", bordercolor=BORDER,
                            lightcolor=BORDER, darkcolor=BORDER, arrowcolor=TEXT,
                            insertcolor=TEXT, selectbackground="#455A53", selectforeground=TEXT,
                            padding=(4, 4))
            style.map(widget, fieldbackground=[("readonly", PANEL), ("disabled", BACKGROUND)],
                      foreground=[("disabled", "#899C95"), ("readonly", TEXT)],
                      bordercolor=[("focus", ACCENT)])
        style.configure("TScrollbar", background="#455A53", troughcolor=BACKGROUND,
                        lightcolor=BORDER, darkcolor=BORDER, bordercolor=BORDER,
                        borderwidth=0, arrowcolor=TEXT)
        style.configure("TSeparator", background=BORDER)
        self.option_add("*TCombobox*Listbox.background", PANEL)
        self.option_add("*TCombobox*Listbox.foreground", TEXT)
        self.option_add("*TCombobox*Listbox.selectBackground", "#455A53")

    def build_window(self):
        main = ttk.Frame(self, padding=(20, 16))
        main.pack(fill="both", expand=True)
        main.columnconfigure(0, weight=1)
        main.rowconfigure(3, weight=1)

        ttk.Label(main, text="Маршруты по кампусу", style="Title.TLabel").grid(
            row=0, column=0, sticky="w", pady=(0, 14))
        self.build_route_controls(main)
        ttk.Separator(main).grid(row=2, column=0, sticky="ew", pady=(12, 10))

        content = ttk.Frame(main)
        content.grid(row=3, column=0, sticky="nsew")
        content.columnconfigure(1, weight=1)
        content.rowconfigure(0, weight=1)
        self.build_editor(content)

        map_frame = ttk.Frame(content)
        map_frame.grid(row=0, column=1, sticky="nsew", padx=(16, 0))
        map_frame.columnconfigure(0, weight=1)
        map_frame.rowconfigure(1, weight=1)
        ttk.Label(map_frame, textvariable=self.summary, style="Section.TLabel").grid(
            row=0, column=0, sticky="w", pady=(0, 7))
        self.canvas = tk.Canvas(map_frame, background=PANEL, highlightthickness=1,
                                highlightbackground=BORDER)
        self.canvas.grid(row=1, column=0, sticky="nsew")
        self.canvas.bind("<Configure>", lambda event: self.draw_graph())

        result_header = ttk.Frame(main)
        result_header.grid(row=4, column=0, sticky="ew", pady=(9, 5))
        ttk.Label(result_header, text="Результат", style="Section.TLabel").pack(side="left")
        ttk.Button(result_header, text="Большой граф", command=self.show_benchmark).pack(side="right")
        result_area = ttk.Frame(main)
        result_area.grid(row=5, column=0, sticky="ew")
        result_area.columnconfigure(0, weight=1)
        result_frame = ttk.Frame(result_area)
        result_frame.grid(row=0, column=0, sticky="nsew", padx=(0, 16))
        self.result = self.make_text(result_frame, height=3, wrap="word")
        self.result.configure(state="disabled", font=("Helvetica", 12))

        compare_actions = ttk.Frame(result_area)
        compare_actions.grid(row=0, column=1, sticky="s")
        self.add_entry(compare_actions, 0, "Повторы", self.repeats)
        self.csv_button = ttk.Button(compare_actions, text="Сохранить замер в CSV", command=self.save_csv)
        self.csv_button.grid(row=1, column=1, sticky="w")
        self.status_label = ttk.Label(main, textvariable=self.status, style="Muted.TLabel", wraplength=830)
        self.status_label.grid(row=6, column=0, sticky="w", pady=(7, 0))

    def build_route_controls(self, parent):
        controls = ttk.Frame(parent)
        controls.grid(row=1, column=0, sticky="ew")
        controls.columnconfigure((0, 1), weight=1)
        self.from_box = self.add_combo(controls, 0, "Откуда", self.source, 19)
        self.to_box = self.add_combo(controls, 1, "Куда", self.destination, 19)
        self.algorithm_box = self.add_combo(controls, 2, "Алгоритм", self.algorithm, 16, tuple(ALGORITHMS))
        self.route_button = ttk.Button(controls, text="Найти маршрут", style="Accent.TButton",
                                       command=self.find_route)
        self.route_button.grid(row=1, column=3, padx=(0, 8), sticky="ew")
        self.groups_button = ttk.Button(controls, text="Связность", command=self.find_groups)
        self.groups_button.grid(row=1, column=4, padx=(0, 8), sticky="ew")
        self.compare_button = ttk.Button(controls, text="Сравнить", command=self.start_comparison)
        self.compare_button.grid(row=1, column=5, sticky="ew")

    def add_combo(self, parent, column, title, value, width, choices=()):
        ttk.Label(parent, text=title, style="Muted.TLabel").grid(
            row=0, column=column, sticky="w", pady=(0, 4))
        combo = ttk.Combobox(parent, textvariable=value, values=choices, width=width, state="readonly")
        combo.grid(row=1, column=column, sticky="ew", padx=(0, 8))
        return combo

    def build_editor(self, parent):
        editor_frame = ttk.Frame(parent)
        editor_frame.grid(row=0, column=0, sticky="ns")
        editor_frame.columnconfigure(0, weight=1)
        editor_frame.rowconfigure(2, weight=1)
        ttk.Label(editor_frame, text="Исходные данные", style="Section.TLabel").grid(
            row=0, column=0, sticky="w")
        ttk.Label(editor_frame, text="Число корпусов и дорожек, затем названия.\n"
                                     "Дорожки в формате «откуда куда длина».",
                  style="Muted.TLabel", justify="left").grid(row=1, column=0, sticky="w", pady=(4, 8))
        text_frame = ttk.Frame(editor_frame)
        text_frame.grid(row=2, column=0, sticky="nsew")
        self.editor = self.make_text(text_frame, width=34, height=7, undo=True)
        self.editor.bind("<<Modified>>", self.editor_changed)
        self.editor.bind("<Control-Return>", self.apply_graph_shortcut)
        if self.tk.call("tk", "windowingsystem") == "aqua":
            self.editor.bind("<Command-Return>", self.apply_graph_shortcut)

        edit_actions = ttk.Frame(editor_frame)
        edit_actions.grid(row=3, column=0, sticky="ew", pady=(8, 10))
        ttk.Button(edit_actions, text="Применить", command=self.apply_graph).pack(side="left")
        ttk.Button(edit_actions, text="Вернуть пример", command=self.load_example).pack(
            side="left", padx=(7, 0))

        ttk.Separator(editor_frame).grid(row=4, column=0, sticky="ew", pady=(0, 10))
        ttk.Label(editor_frame, text="Генератор схем", style="Section.TLabel").grid(
            row=5, column=0, sticky="w", pady=(0, 6))
        generator = ttk.Frame(editor_frame)
        generator.grid(row=6, column=0, sticky="ew")
        generator.columnconfigure(0, weight=1)
        topology_box = self.add_combo(generator, 0, "Топология", self.topology, 16, tuple(TOPOLOGIES))
        topology_box.bind("<<ComboboxSelected>>", self.topology_changed)
        self.add_entry(generator, 1, "Корпуса", self.size)
        self.group_entry = self.add_entry(generator, 2, "Группы", self.group_count)
        self.topology_changed()
        generator_actions = ttk.Frame(editor_frame)
        generator_actions.grid(row=7, column=0, sticky="ew", pady=(6, 0))
        self.add_entry(generator_actions, 0, "Seed", self.seed, width=9)
        ttk.Button(generator_actions, text="Сгенерировать", command=self.generate_graph).grid(
            row=1, column=1, sticky="w")

    def add_entry(self, parent, column, title, variable, width=6):
        ttk.Label(parent, text=title, style="Muted.TLabel").grid(
            row=0, column=column, sticky="w", pady=(0, 4))
        entry = ttk.Entry(parent, textvariable=variable, width=width)
        entry.grid(row=1, column=column, sticky="ew", padx=(0, 8))
        return entry

    def make_text(self, parent, **options):
        parent.columnconfigure(0, weight=1)
        parent.rowconfigure(0, weight=1)
        text = tk.Text(parent, background=PANEL, foreground=TEXT, insertbackground=ACCENT,
                       selectbackground="#455A53", selectforeground=TEXT,
                       highlightthickness=1, highlightbackground=BORDER, highlightcolor=ACCENT,
                       borderwidth=0, padx=8, pady=6, font=("Menlo", 11), **options)
        scroll = ttk.Scrollbar(parent, orient="vertical", command=text.yview)
        text.configure(yscrollcommand=scroll.set)
        text.grid(row=0, column=0, sticky="nsew")
        scroll.grid(row=0, column=1, sticky="ns")
        return text

    def set_status(self, message="", error=False):
        self.status.set(message)
        self.status_label.configure(style="Error.TLabel" if error else "Muted.TLabel")

    def set_result(self, message):
        self.result.configure(state="normal")
        self.result.delete("1.0", "end")
        self.result.insert("1.0", message)
        self.result.configure(state="disabled")

    def editor_text(self):
        return self.editor.get("1.0", "end-1c")

    def editor_changed(self, event):
        if self.editor.edit_modified():
            self.editor.edit_modified(False)
            self.update_buttons()
            if self.editor_text() != self.graph_text:
                self.set_status("Нажмите «Применить», чтобы обновить схему.")
            else:
                self.set_status()

    def update_buttons(self):
        ready = self.editor_text() == self.graph_text and self.comparison is None
        empty = not self.vertices
        for button in (self.route_button, self.compare_button):
            button.configure(state="normal" if ready and not empty else "disabled")
        self.groups_button.configure(state="normal" if ready else "disabled")
        self.csv_button.configure(state="normal" if ready and self.last_timings else "disabled")

    def topology_changed(self, event=None):
        state = "normal" if TOPOLOGIES[self.topology.get()] == "separate" else "disabled"
        self.group_entry.configure(state=state)

    def vertex_label(self, vertex):
        return f"{vertex} · {self.vertices[vertex]}"

    def accept_graph(self, graph):
        data = json.loads(run_core(self.program, "show", graph=graph))
        self.vertices = data["vertices"]
        self.edges = data["edges"]
        self.graph_text = graph
        if self.editor_text() != graph:
            self.editor.delete("1.0", "end")
            self.editor.insert("1.0", graph)
            self.editor.edit_reset()
            self.editor.edit_modified(False)

        options = [self.vertex_label(vertex) for vertex in range(len(self.vertices))]
        self.from_box.configure(values=options, state="readonly" if options else "disabled")
        self.to_box.configure(values=options, state="readonly" if options else "disabled")
        if options:
            self.from_box.current(0)
            self.to_box.current(len(options) - 1)
        else:
            self.source.set("")
            self.destination.set("")
        self.path = []
        self.groups = []
        self.last_timings = None
        self.summary.set(f"Схема кампуса · {len(self.vertices)} корп. · {len(self.edges)} дор.")
        self.set_status()
        self.set_result("Выберите корпуса для поиска маршрута." if options else "Схема пуста.")
        self.update_buttons()
        self.draw_graph()

    def apply_graph_shortcut(self, event):
        self.apply_graph()
        return "break"

    def apply_graph(self):
        try:
            self.accept_graph(self.editor_text())
        except (ValueError, OSError, subprocess.SubprocessError) as error:
            self.set_status(str(error), error=True)

    def load_example(self):
        try:
            self.accept_graph(run_core(self.program, "demo"))
            self.to_box.current(3)
            self.find_route()
        except (ValueError, OSError, subprocess.SubprocessError) as error:
            self.set_status(str(error), error=True)

    def generate_graph(self):
        topology = TOPOLOGIES[self.topology.get()]
        args = [topology, self.size.get().strip(), self.seed.get().strip()]
        if topology == "separate":
            args.append(self.group_count.get().strip())
        try:
            self.accept_graph(run_core(self.program, "generate", *args))
        except (ValueError, OSError, subprocess.SubprocessError) as error:
            self.set_status(str(error), error=True)

    def find_route(self):
        if self.editor_text() != self.graph_text:
            return
        try:
            data = json.loads(run_core(
                self.program, "route", ALGORITHMS[self.algorithm.get()], self.from_box.current(),
                self.to_box.current(), graph=self.graph_text,
            ))
            self.path = data["path"]
            self.groups = []
            self.draw_graph()
            self.set_status()
            if self.path:
                route = " → ".join(self.vertices[vertex] for vertex in self.path)
                self.set_result(f"{self.algorithm.get()} · {data['distance']} м\n{route}")
            else:
                self.set_result(f"{self.algorithm.get()} · Маршрут недоступен\n"
                                f"{self.source.get()} → {self.destination.get()}")
        except (ValueError, OSError, subprocess.SubprocessError) as error:
            self.set_status(str(error), error=True)

    def find_groups(self):
        if self.editor_text() != self.graph_text:
            return
        try:
            data = json.loads(run_core(self.program, "components", graph=self.graph_text))
            self.groups = data["groups"]
            self.path = []
            self.draw_graph()
            groups = [
                f"{index}. " + ", ".join(self.vertex_label(vertex) for vertex in group)
                for index, group in enumerate(self.groups, 1)
            ]
            self.set_result("\n".join([f"Связанных групп — {len(groups)}", *groups]))
            self.set_status()
        except (ValueError, OSError, subprocess.SubprocessError) as error:
            self.set_status(str(error), error=True)

    def start_comparison(self):
        if self.comparison is not None or self.editor_text() != self.graph_text:
            return
        source = self.from_box.current()
        repeats = self.repeats.get().strip()
        graph = self.graph_text
        self.comparison = self.worker.submit(run_core, self.program, "compare", source, repeats, graph=graph)
        self.update_buttons()
        self.set_status("Сравниваю время поиска до всех достижимых корпусов…")
        self.after(50, self.finish_comparison, source, repeats, graph)

    def finish_comparison(self, source, repeats, graph):
        if not self.comparison.done():
            self.after(50, self.finish_comparison, source, repeats, graph)
            return
        future, self.comparison = self.comparison, None
        try:
            times = json.loads(future.result())
            if graph == self.graph_text and graph == self.editor_text():
                self.last_timings = (len(self.vertices), len(self.edges), source, int(repeats), times)
                self.set_result(f"Старт — {self.vertex_label(source)} · Повторов — {repeats}\n"
                                f"Дейкстра — {times['dijkstra_us']:.3f} мкс\n"
                                f"Беллман–Форд — {times['bellman_ford_us']:.3f} мкс")
                self.set_status("Медианное время · Расстояния, найденные алгоритмами, совпали.")
            elif self.editor_text() == self.graph_text:
                self.set_status()
        except (ValueError, OSError, subprocess.SubprocessError) as error:
            self.set_status(str(error), error=True)
        finally:
            self.update_buttons()

    def save_csv(self):
        if not self.last_timings:
            return
        destination = filedialog.asksaveasfilename(
            title="Сохранить результаты сравнения", initialfile="campus_routes.csv",
            defaultextension=".csv", filetypes=[("CSV", "*.csv")],
        )
        if not destination:
            return
        vertices, edges, source, repeats, times = self.last_timings
        try:
            with open(destination, "w", encoding="utf-8", newline="") as file:
                writer = csv.writer(file)
                writer.writerow(["vertices", "edges", "source", "repeats", "dijkstra_us", "bellman_ford_us"])
                writer.writerow([vertices, edges, source, repeats, times["dijkstra_us"], times["bellman_ford_us"]])
            self.set_status(f"Сохранено в {Path(destination).name}")
        except OSError as error:
            self.set_status(str(error), error=True)

    def show_benchmark(self):
        if self.benchmark_window is None:
            self.benchmark_window = BenchmarkWindow(self)
        self.benchmark_window.deiconify()
        self.benchmark_window.lift()

    def graph_positions(self, width, height):
        count = len(self.vertices)
        if count == 1:
            return [(width / 2, height / 2)]
        radius_x = max(20, width / 2 - 90)
        radius_y = max(20, height / 2 - 58)
        return [
            (width / 2 + radius_x * math.cos(math.tau * index / count - math.pi / 2),
             height / 2 + radius_y * math.sin(math.tau * index / count - math.pi / 2))
            for index in range(count)
        ]

    def draw_graph(self):
        canvas = self.canvas
        canvas.delete("all")
        width, height = canvas.winfo_width(), canvas.winfo_height()
        if width <= 1 or height <= 1:
            return

        if not self.vertices or len(self.vertices) > 24 or len(self.edges) > 90:
            message = ("Введите схему или создайте её генератором." if not self.vertices else
                       "Схема слишком большая для рисунка.\nДанные и алгоритмы доступны.")
            canvas.create_text(width / 2, height / 2, text=message, fill=MUTED,
                               font=("Helvetica", 12), justify="center")
            return

        positions = self.graph_positions(width, height)
        path_edges = {tuple(sorted(edge)) for edge in zip(self.path, self.path[1:])}
        path_vertices = set(self.path)
        group_colors = {
            vertex: GROUP_COLORS[index % len(GROUP_COLORS)]
            for index, group in enumerate(self.groups) for vertex in group
        }

        self.draw_edges(positions, path_edges, group_colors)
        for vertex, (x, y) in enumerate(positions):
            color = ACCENT if vertex in path_vertices else group_colors.get(vertex, BORDER)
            canvas.create_oval(x - 15, y - 15, x + 15, y + 15,
                               fill="#253530", outline=color, width=2)
            canvas.create_text(x, y, text=str(vertex), fill=TEXT, font=("Helvetica", 11, "bold"))
            name = self.vertices[vertex]
            if len(name) > 19:
                name = name[:18] + "…"
            if len(positions) <= 12:
                canvas.create_text(x, y + 20, text=name, fill=TEXT, font=("Helvetica", 11),
                                   width=115, anchor="n", justify="center")

    def draw_edges(self, positions, path_edges, group_colors):
        canvas = self.canvas
        ordered_edges = sorted(self.edges, key=lambda edge: tuple(edge[:2]) in path_edges)
        for first, second, _ in ordered_edges:
            x1, y1 = positions[first]
            x2, y2 = positions[second]
            on_path = (first, second) in path_edges
            color = ACCENT if on_path else group_colors.get(first, "#718880")
            canvas.create_line(x1, y1, x2, y2, fill=color, width=4 if on_path else 2)

        for first, second, weight in ordered_edges:
            x1, y1 = positions[first]
            x2, y2 = positions[second]
            label = canvas.create_text((x1 + x2) / 2, (y1 + y2) / 2, text=f"{weight:.5g}",
                                       fill=TEXT, font=("Helvetica", 10))
            left, top, right, bottom = canvas.bbox(label)
            background = canvas.create_rectangle(left - 3, top, right + 3, bottom,
                                                 fill=PANEL, outline="")
            canvas.tag_lower(background, label)

    def close(self):
        if self.comparison is None:
            self.folder.cleanup()
        else:
            folder = self.folder
            self.comparison.add_done_callback(lambda _: folder.cleanup())
        self.worker.shutdown(wait=False, cancel_futures=True)
        self.destroy()


class BenchmarkWindow(tk.Toplevel):
    def __init__(self, app):
        super().__init__(app)
        self.app = app
        self.title("Замер на большом графе")
        self.configure(background=BACKGROUND)
        self.resizable(False, False)
        self.protocol("WM_DELETE_WINDOW", self.withdraw)

        self.topology = tk.StringVar(value="Цепочка")
        self.size = tk.StringVar(value="4096")
        self.seed = tk.StringVar(value="42")
        self.repeats = tk.StringVar(value="7")
        self.status = tk.StringVar()
        self.measurements = []

        body = ttk.Frame(self, padding=18)
        body.pack(fill="both", expand=True)
        controls = ttk.Frame(body)
        controls.pack(fill="x", pady=(0, 10))
        app.add_combo(controls, 0, "Топология", self.topology, 19, tuple(TOPOLOGIES))
        app.add_entry(controls, 1, "Вершины", self.size, width=7)
        app.add_entry(controls, 2, "Seed", self.seed, width=8)
        app.add_entry(controls, 3, "Повторы", self.repeats, width=6)

        style = ttk.Style(self)
        style.configure("Benchmark.Treeview", background=PANEL, fieldbackground=PANEL,
                        foreground=TEXT, rowheight=26)
        style.configure("Benchmark.Treeview.Heading", background="#35433F", foreground=TEXT)
        columns = ("algorithm", "time", "memory")
        self.table = ttk.Treeview(body, columns=columns, show="headings", height=2,
                                  style="Benchmark.Treeview")
        for column, title, width in zip(
            columns, ("Алгоритм", "Медиана, мкс", "Пик RSS, МиБ"), (160, 130, 135),
        ):
            self.table.heading(column, text=title)
            self.table.column(column, width=width, anchor="w" if column == "algorithm" else "e")
        self.table.pack(fill="x")

        actions = ttk.Frame(body)
        actions.pack(fill="x", pady=(10, 0))
        self.start_button = ttk.Button(actions, text="Сравнить", style="Accent.TButton", command=self.start)
        self.start_button.pack(side="left")
        self.export_button = ttk.Button(actions, text="Экспорт CSV", command=self.save_csv, state="disabled")
        self.export_button.pack(side="left", padx=(8, 0))
        self.status_label = ttk.Label(body, textvariable=self.status, style="Muted.TLabel", wraplength=425)
        self.status_label.pack(fill="x", pady=(8, 0))
        self.set_status("До 4096 вершин. Пик памяти учитывает матрицу графа.")

    def set_status(self, message, error=False):
        self.status.set(message)
        self.status_label.configure(style="Error.TLabel" if error else "Muted.TLabel")

    def start(self):
        if self.app.comparison is not None:
            self.set_status("Дождитесь завершения текущего сравнения.")
            return
        try:
            size = int(self.size.get())
            seed = int(self.seed.get())
            repeats = int(self.repeats.get())
        except ValueError:
            self.set_status("Число вершин, seed и повторы должны быть целыми числами.", error=True)
            return
        if not 3 <= size <= 4096 or not 0 <= seed <= 4294967295 or not 1 <= repeats <= 50:
            self.set_status("Вершины от 3 до 4096, seed от 0 до 4294967295, повторы от 1 до 50.", error=True)
            return
        if not self.app.program.is_file():
            self.set_status("Сначала загрузите пример, чтобы собрать C++-адаптер.", error=True)
            return

        topology = TOPOLOGIES[self.topology.get()]
        self.app.comparison = self.app.worker.submit(
            compare_large_graph, self.app.program, topology, size, seed, repeats,
        )
        self.app.update_buttons()
        self.start_button.configure(state="disabled")
        self.set_status(f"Замер на {size} вершинах, по {repeats} запусков…")
        self.after(50, self.finish)

    def finish(self):
        future = self.app.comparison
        if not future.done():
            self.after(50, self.finish)
            return
        self.app.comparison = None
        try:
            rows = future.result()
            self.table.delete(*self.table.get_children())
            for row in rows:
                self.table.insert("", "end", values=(
                    BENCHMARK_ALGORITHMS[row["algorithm"]], f"{row['median_us']:.3f}",
                    f"{row['peak_rss_bytes'] / (1024 * 1024):.2f}",
                ))
            self.measurements.extend(rows)
            self.export_button.configure(state="normal")
            self.set_status(f"{rows[0]['vertices']} вершин · Расстояния обоих алгоритмов совпали.")
        except (ValueError, OSError, subprocess.SubprocessError) as error:
            self.set_status(str(error), error=True)
        finally:
            self.start_button.configure(state="normal")
            self.app.update_buttons()

    def save_csv(self):
        destination = filedialog.asksaveasfilename(
            parent=self, title="Сохранить замеры", initialfile="large_graph_benchmark.csv",
            defaultextension=".csv", filetypes=[("CSV", "*.csv")],
        )
        if not destination:
            return
        try:
            with open(destination, "w", encoding="utf-8", newline="") as file:
                writer = csv.DictWriter(file, fieldnames=CSV_FIELDS)
                writer.writeheader()
                writer.writerows(self.measurements)
            self.set_status(f"Сохранено в {Path(destination).name}")
        except OSError as error:
            self.set_status(str(error), error=True)


if __name__ == "__main__":
    try:
        App().mainloop()
    except tk.TclError as error:
        raise SystemExit(f"Не удалось открыть Tkinter: {error}") from error
