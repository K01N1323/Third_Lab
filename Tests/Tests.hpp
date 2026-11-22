#ifndef TESTS_HPP
#define TESTS_HPP

#include <cmath>
#include <limits>
#include <sstream>

#include "TestHelpers.hpp"
#include "Include/RouteMeasurement.hpp"
#include "Ui/GraphIO.hpp"

template <typename TVertex, typename TWeight>
bool CheckWeight(const Graph<TVertex, TWeight>& graph, int from, int to, TWeight expected) {
    const TWeight* weight = graph.FindWeight(from, to);
    return weight != nullptr && *weight == expected;
}

inline bool CheckVertices(const MutableArraySequence<int>& vertices, const int* expected, int count) {
    if (vertices.GetLength() != count) {return false;}
    for (int index = 0; index < count; index++) {
        if (vertices.Get(index) != expected[index]) {return false;}
    }
    return true;
}

inline void RunGraphTests(int& tests_passed, int& tests_failed) {
    Graph<std::string, int> graph(3);
    graph.SetVertex(0, "Проходная");
    graph.AddEdge(0, 1, 0);
    CheckGraph(graph.GetVertex(0) == "Проходная" &&
               CheckWeight(graph, 0, 1, 0) && CheckWeight(graph, 1, 0, 0) &&
               graph.FindWeight(0, 2) == nullptr,
               "Симметричная матрица отличает нулевой вес от отсутствия ребра",
               tests_passed, tests_failed);
    CheckGraph(graph.GetEdges().GetLength() == 1,
               "При выводе каждое неориентированное ребро встречается один раз",
               tests_passed, tests_failed);
    CheckGraph(TestThrows<std::invalid_argument>([&]() {graph.AddEdge(1, 0, 5);}) &&
               TestThrows<std::invalid_argument>([&]() {graph.AddEdge(0, 0, 1);}) &&
               TestThrows<std::invalid_argument>([&]() {graph.AddEdge(0, 2, -1);}) &&
               TestThrows<std::out_of_range>([&]() {graph.AddEdge(-1, 1, 2);}) &&
               TestThrows<std::out_of_range>([&]() {graph.GetVertex(3);}) &&
               graph.GetEdges().GetLength() == 1,
               "Повтор, петля, отрицательный вес и неверные индексы отклоняются",
               tests_passed, tests_failed);
    auto copy = graph;
    graph.RemoveEdge(1, 0);
    CheckGraph(graph.FindWeight(0, 1) == nullptr && graph.FindWeight(1, 0) == nullptr &&
               CheckWeight(copy, 1, 0, 0) && graph.GetVertex(0) == "Проходная",
               "Удаление симметрично, данные вершины и копия сохраняются",
               tests_passed, tests_failed);

    Graph<int, double> real(2);
    CheckGraph(TestThrows<std::invalid_argument>([&]() {
                   real.AddEdge(0, 1, std::numeric_limits<double>::infinity());
               }) &&
               TestThrows<std::invalid_argument>([&]() {
                   real.AddEdge(0, 1, std::numeric_limits<double>::quiet_NaN());
               }) &&
               TestThrows<std::invalid_argument>([]() {Graph<int, int> invalid(-1);}) &&
               TestThrows<std::invalid_argument>([]() {Graph<int, int> invalid(50000);}),
               "Неконечные веса и недопустимые размеры отклоняются",
               tests_passed, tests_failed);
}

template <typename TVertex, typename TWeight>
bool CheckPath(const Graph<TVertex, TWeight>& graph, const ShortestPaths& paths,
               int source, int destination) {
    auto path = paths.GetPath(destination);
    long double expected = paths.GetDistance(destination);
    if (expected < 0) {return path.GetLength() == 0;}
    if (path.GetLength() == 0 || path.GetFirst() != source || path.GetLast() != destination ||
        path.GetLength() > graph.GetVertexCount()) {return false;}

    long double distance = 0;
    for (int index = 1; index < path.GetLength(); index++) {
        const TWeight* weight = graph.FindWeight(path.Get(index - 1), path.Get(index));
        if (weight == nullptr) {return false;}
        distance += *weight;
    }
    return distance == expected;
}

inline void RunRouteTests(int& tests_passed, int& tests_failed) {
    Graph<int, double> graph(6);
    graph.AddEdge(0, 1, 8);
    graph.AddEdge(0, 2, 0);
    graph.AddEdge(2, 1, 1.5);
    graph.AddEdge(2, 3, 1.5);
    graph.AddEdge(1, 4, 2.25);
    graph.AddEdge(3, 4, 2.25);
    graph.AddEdge(1, 3, 0);

    const int origin[] = {0};
    const auto algorithms = {GraphSearch::Dijkstra<int, double>, GraphSearch::BellmanFord<int, double>};
    for (auto search : algorithms) {
        auto paths = search(graph, 0);
        CheckGraph(paths.GetDistance(1) == 1.5L && paths.GetDistance(2) == 0 &&
                   paths.GetDistance(4) == 3.75L && CheckPath(graph, paths, 0, 4),
                   "Обход через другие вершины короче прямого ребра, равные пути допустимы",
                   tests_passed, tests_failed);
        CheckGraph(paths.GetDistance(0) == 0 && CheckVertices(paths.GetPath(0), origin, 1) &&
                   paths.GetDistance(5) < 0 && paths.GetPath(5).GetLength() == 0,
                   "Путь в себя имеет длину 0, до изолированной вершины пути нет",
                   tests_passed, tests_failed);
        auto reverse = search(graph, 4);
        CheckGraph(reverse.GetDistance(0) == 3.75L && CheckPath(graph, reverse, 4, 0),
                   "Дорожки работают в обе стороны",
                   tests_passed, tests_failed);
        CheckGraph(TestThrows<std::out_of_range>([&]() {search(graph, 6);}) &&
                   TestThrows<std::out_of_range>([&]() {search(Graph<int, double>(0), 0);}),
                   "Поиск отклоняет старт вне схемы и старт в пустой схеме",
                   tests_passed, tests_failed);
    }

    Graph<int, int> reverseChain(8);
    for (int vertex = 1; vertex < 8; vertex++) {reverseChain.AddEdge(vertex - 1, vertex, 2);}
    auto paths = GraphSearch::BellmanFord(reverseChain, 7);
    CheckGraph(paths.GetDistance(0) == 14 && paths.GetPath(0).GetLength() == 8 &&
               CheckPath(reverseChain, paths, 7, 0),
               "Беллман–Форд выполняет все нужные проходы в обратной цепочке",
               tests_passed, tests_failed);

    Graph<int, long double> large(3);
    long double max = std::numeric_limits<long double>::max();
    large.AddEdge(0, 1, max);
    large.AddEdge(1, 2, max);
    CheckGraph(TestThrows<std::runtime_error>([&]() {GraphSearch::Dijkstra(large, 0);}) &&
               TestThrows<std::runtime_error>([&]() {GraphSearch::BellmanFord(large, 0);}),
               "Недопустимо большую длину достижимого маршрута отличаю от отсутствия пути",
               tests_passed, tests_failed);
    large.AddEdge(0, 2, max);
    CheckGraph(GraphSearch::Dijkstra(large, 0).GetDistance(2) == max &&
               GraphSearch::BellmanFord(large, 0).GetDistance(2) == max,
               "Слишком длинный обход не мешает найти представимый маршрут",
               tests_passed, tests_failed);
}

inline void RunComponentTests(int& tests_passed, int& tests_failed) {
    Graph<int, int> graph(6);
    graph.AddEdge(0, 2, 0);
    graph.AddEdge(2, 4, 2);
    graph.AddEdge(1, 3, 1);
    const int first[] = {0, 2, 4};
    const int second[] = {1, 3};
    const int third[] = {5};
    auto groups = GraphSearch::FindComponents(graph);
    CheckGraph(groups.GetLength() == 3 && CheckVertices(groups.Get(0), first, 3) &&
               CheckVertices(groups.Get(1), second, 2) && CheckVertices(groups.Get(2), third, 1),
               "Обход включает нулевые рёбра и выделяет изолированный корпус",
               tests_passed, tests_failed);
    graph.RemoveEdge(0, 2);
    groups = GraphSearch::FindComponents(graph);
    const int isolated[] = {0};
    const int remaining[] = {2, 4};
    CheckGraph(groups.GetLength() == 4 && CheckVertices(groups.Get(0), isolated, 1) &&
               CheckVertices(groups.Get(1), second, 2) && CheckVertices(groups.Get(2), remaining, 2) &&
               CheckVertices(groups.Get(3), third, 1),
               "Удаление связи меняет состав групп",
               tests_passed, tests_failed);
    auto single = GraphSearch::FindComponents(Graph<int, int>(1));
    CheckGraph(GraphSearch::FindComponents(Graph<int, int>(0)).GetLength() == 0 &&
               single.GetLength() == 1 && CheckVertices(single.Get(0), isolated, 1),
               "Пустая схема и схема из одного корпуса",
               tests_passed, tests_failed);
}

inline void RunGeneratorTests(int& tests_passed, int& tests_failed) {
    auto chain = GenerateGraph(GraphTopology::Chain, 8, 42);
    auto sameChain = GenerateGraph(GraphTopology::Chain, 8, 42);
    std::ostringstream first;
    std::ostringstream second;
    WriteGraph(first, chain);
    WriteGraph(second, sameChain);
    CheckGraph(chain.GetEdges().GetLength() == 7 && first.str() == second.str() &&
               chain.FindWeight(0, 1) != nullptr && chain.FindWeight(0, 7) == nullptr,
               "Цепочка имеет нужный размер и воспроизводимые веса",
               tests_passed, tests_failed);

    auto cycle = GenerateGraph(GraphTopology::Cycle, 8, 42);
    auto separate = GenerateGraph(GraphTopology::SeparateChains, 8, 42, 3);
    CheckGraph(cycle.GetEdges().GetLength() == 8 && cycle.FindWeight(0, 7) != nullptr &&
               GraphSearch::FindComponents(cycle).GetLength() == 1,
               "Кольцо замыкает цепочку",
               tests_passed, tests_failed);
    auto groups = GraphSearch::FindComponents(separate);
    const int firstGroup[] = {0, 1, 2};
    const int secondGroup[] = {3, 4, 5};
    const int thirdGroup[] = {6, 7};
    CheckGraph(separate.GetEdges().GetLength() == 5 && groups.GetLength() == 3 &&
               CheckVertices(groups.Get(0), firstGroup, 3) &&
               CheckVertices(groups.Get(1), secondGroup, 3) &&
               CheckVertices(groups.Get(2), thirdGroup, 2),
               "Генератор строит заданное количество отдельных цепочек",
               tests_passed, tests_failed);
    CheckGraph(TestThrows<std::invalid_argument>([]() {GenerateGraph(GraphTopology::Cycle, 2, 0);}) &&
               TestThrows<std::invalid_argument>([]() {GenerateGraph(GraphTopology::SeparateChains, 2, 0, 3);}) &&
               TestThrows<std::invalid_argument>([]() {GenerateGraph(GraphTopology::Chain, -1, 0);}) &&
               GenerateGraph(GraphTopology::Chain, 0, 0).GetVertexCount() == 0,
               "Генератор проверяет ограничения каждой топологии",
               tests_passed, tests_failed);

    bool agree = true;
    for (const auto* graph : {&chain, &cycle, &separate}) {
        for (int source = 0; source < graph->GetVertexCount(); source++) {
            auto dijkstra = GraphSearch::Dijkstra(*graph, source);
            auto bellman = GraphSearch::BellmanFord(*graph, source);
            agree = agree && EqualDistances(dijkstra, bellman);
            for (int target = 0; target < graph->GetVertexCount(); target++) {
                agree = agree && CheckPath(*graph, dijkstra, source, target) &&
                        CheckPath(*graph, bellman, source, target);
            }
        }
    }
    CheckGraph(agree, "Алгоритмы дают одинаковые длины и корректные пути на трёх топологиях",
               tests_passed, tests_failed);
}

inline void RunInputTests(int& tests_passed, int& tests_failed) {
    auto demo = DemoGraph();
    auto result = FindRoute(demo, RouteAlgorithm::Dijkstra, 0, 3);
    const int expected[] = {0, 1, 2, 3};
    CheckGraph(CheckVertices(result.path, expected, 4) && result.distance == 90,
               "В демонстрационной схеме обход занимает 90 м вместо прямых 120 м",
               tests_passed, tests_failed);
    CheckGraph(TestThrows<std::invalid_argument>([]() {
                   std::istringstream input("3 2\nA\nB\nC\n0 1 2\n1 0 3\n");
                   ReadGraph(input);
               }) &&
               TestThrows<std::invalid_argument>([]() {
                   std::istringstream input("2 1\nA\nB\n0 1\n");
                   ReadGraph(input);
               }) &&
               TestThrows<std::invalid_argument>([]() {
                   std::istringstream input("1 0\n  \n");
                   ReadGraph(input);
               }),
               "При ручном вводе проверяются названия, формат и повторы",
               tests_passed, tests_failed);

    auto times = CompareRoutes(demo, 0, 2);
    std::ostringstream csv;
    WritePerformance(csv, 4, 1, 9);
    CheckGraph(std::isfinite(times.dijkstra) && times.dijkstra >= 0 &&
               std::isfinite(times.bellmanFord) && times.bellmanFord >= 0 &&
               csv.str().find("separate,4,2,3,1,9,") != std::string::npos &&
               TestThrows<std::invalid_argument>([&]() {CompareRoutes(demo, 0, 0);}),
               "Сравнение измеряет оба алгоритма и записывает настройки в CSV",
               tests_passed, tests_failed);
}

inline int RunTests() {
    int tests_passed = 0;
    int tests_failed = 0;
    RunGraphTests(tests_passed, tests_failed);
    RunRouteTests(tests_passed, tests_failed);
    RunComponentTests(tests_passed, tests_failed);
    RunGeneratorTests(tests_passed, tests_failed);
    RunInputTests(tests_passed, tests_failed);
    std::cout << "\nУспешно " << tests_passed << ", провалено " << tests_failed << '\n';
    return tests_failed;
}

#endif // TESTS_HPP
