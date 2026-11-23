#include "GraphIO.hpp"

#include <iomanip>
#include <limits>
#include <sstream>

static const char* Demo = R"(7 7
Проходная
Библиотека
Столовая
Главный корпус
Лаборатория
Спортзал
Архив
0 1 40
1 2 30
2 3 20
3 4 25
4 5 35
0 3 120
0 5 95
)";

static std::string ReadLine(std::istream& input, const std::string& error) {
    std::string line;
    if (!std::getline(input, line)) {throw std::invalid_argument(error);}
    if (!line.empty() && line.back() == '\r') {line.pop_back();}
    return line;
}

void CheckCampusSize(int count) {
    if (count < 0 || count > MaxCampusSize) {
        throw std::invalid_argument("Количество корпусов должно быть от 0 до " +
                                    std::to_string(MaxCampusSize));
    }
}

CampusGraph ReadGraph(std::istream& input) {
    std::istringstream header(ReadLine(input, "Не задан размер схемы"));
    int count = 0;
    int edgeCount = 0;
    std::string extra;
    if (!(header >> count >> edgeCount) || (header >> extra)) {
        throw std::invalid_argument("В первой строке укажите количество корпусов и дорожек");
    }
    CheckCampusSize(count);
    if (edgeCount < 0 || edgeCount > count * (count - 1) / 2) {
        throw std::invalid_argument("Некорректное количество дорожек");
    }

    CampusGraph graph(count);
    for (int vertex = 0; vertex < count; vertex++) {
        std::string name = ReadLine(input, "Пропущено название корпуса");
        if (name.find_first_not_of(" \t") == std::string::npos) {
            throw std::invalid_argument("Название корпуса не может быть пустым");
        }
        graph.SetVertex(vertex, name);
    }

    for (int index = 0; index < edgeCount; index++) {
        std::istringstream row(ReadLine(input, "Пропущена строка дорожки"));
        int from = 0;
        int to = 0;
        double weight = 0;
        if (!(row >> from >> to >> weight) || (row >> extra)) {
            throw std::invalid_argument("Дорожка " + std::to_string(index + 1) +
                                        " должна содержать два номера и длину");
        }
        graph.AddEdge(from, to, weight);
    }
    return graph;
}

void WriteGraph(std::ostream& output, const CampusGraph& graph) {
    auto edges = graph.GetEdges();
    output << graph.GetVertexCount() << ' ' << edges.GetLength() << '\n';
    for (int vertex = 0; vertex < graph.GetVertexCount(); vertex++) {
        output << graph.GetVertex(vertex) << '\n';
    }
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (int index = 0; index < edges.GetLength(); index++) {
        const auto& edge = edges.Get(index);
        output << edge.from << ' ' << edge.to << ' ' << edge.weight << '\n';
    }
}

CampusGraph DemoGraph() {
    std::istringstream input(Demo);
    return ReadGraph(input);
}

int ParseInt(const std::string& text) {
    std::istringstream input(text);
    int result = 0;
    std::string extra;
    if (!(input >> result) || (input >> extra)) {
        throw std::invalid_argument("Ожидалось целое число");
    }
    return result;
}

unsigned int ParseSeed(const std::string& text) {
    std::istringstream input(text);
    unsigned int result = 0;
    std::string extra;
    input >> std::ws;
    if (input.peek() == '-' || !(input >> result) || (input >> extra)) {
        throw std::invalid_argument("Начальное число генератора должно быть от 0 до 4294967295");
    }
    return result;
}

GraphTopology ParseTopology(const std::string& name) {
    if (name == "chain") {return GraphTopology::Chain;}
    if (name == "cycle") {return GraphTopology::Cycle;}
    if (name == "separate") {return GraphTopology::SeparateChains;}
    throw std::invalid_argument("Неизвестная топология. Используйте chain, cycle или separate");
}

RouteAlgorithm ParseAlgorithm(const std::string& name) {
    if (name == "dijkstra") {return RouteAlgorithm::Dijkstra;}
    if (name == "bellman-ford") {return RouteAlgorithm::BellmanFord;}
    throw std::invalid_argument("Неизвестный алгоритм. Используйте dijkstra или bellman-ford");
}

RouteResult FindRoute(const CampusGraph& graph, RouteAlgorithm algorithm, int source, int destination) {
    graph.CheckVertex(destination);
    auto paths = algorithm == RouteAlgorithm::Dijkstra
        ? Dijkstra(graph, source)
        : BellmanFord(graph, source);
    return {paths.GetPath(destination), paths.GetDistance(destination)};
}

void WritePerformance(std::ostream& output, int maxSize, int repeats, unsigned int seed) {
    CheckCampusSize(maxSize);
    CheckRepeats(repeats);
    if (maxSize < 3) {throw std::invalid_argument("Для замеров нужно хотя бы три корпуса");}

    const GraphTopology topologies[] = {GraphTopology::Chain, GraphTopology::Cycle,
                                         GraphTopology::SeparateChains};
    const char* names[] = {"chain", "cycle", "separate"};

    output << "topology,vertices,edges,source,repeats,seed,dijkstra_us,bellman_ford_us\n";
    int size = maxSize < 16 ? maxSize : 16;
    while (true) {
        for (int index = 0; index < 3; index++) {
            auto graph = GenerateGraph(topologies[index], size, seed);
            int edgeCount = graph.GetEdges().GetLength();
            int source = size - 1;
            auto times = CompareRoutes(graph, source, repeats);
            output << names[index] << ',' << size << ',' << edgeCount << ',' << source << ','
                   << repeats << ',' << seed << ',' << times.dijkstra << ',' << times.bellmanFord << '\n';
        }
        if (size == maxSize) {break;}
        size = size * 2 < maxSize ? size * 2 : maxSize;
    }
}
