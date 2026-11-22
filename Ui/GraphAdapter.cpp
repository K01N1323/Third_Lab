#include "GraphAdapter.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include "GraphIO.hpp"

static void WriteString(std::ostream& output, const std::string& text) {
    output << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') {output << '\\' << c;}
        else if (c < 32) {
            output << "\\u00" << "0123456789abcdef"[c >> 4]
                   << "0123456789abcdef"[c & 15];
        } else {output << c;}
    }
    output << '"';
}

static void WriteVertices(std::ostream& output, const MutableArraySequence<int>& vertices) {
    output << '[';
    for (int index = 0; index < vertices.GetLength(); index++) {
        if (index > 0) {output << ',';}
        output << vertices.Get(index);
    }
    output << ']';
}

static void WriteGraphJson(std::ostream& output, const CampusGraph& graph) {
    output << "{\"vertices\":[";
    for (int vertex = 0; vertex < graph.GetVertexCount(); vertex++) {
        if (vertex > 0) {output << ',';}
        WriteString(output, graph.GetVertex(vertex));
    }
    output << "],\"edges\":[";
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    auto edges = graph.GetEdges();
    for (int index = 0; index < edges.GetLength(); index++) {
        if (index > 0) {output << ',';}
        const auto& edge = edges.Get(index);
        output << '[' << edge.from << ',' << edge.to << ',' << edge.weight << ']';
    }
    output << "]}\n";
}

static void WriteRouteJson(std::ostream& output, const RouteResult& result) {
    output << "{\"path\":";
    WriteVertices(output, result.path);
    output << ",\"distance\":";
    if (result.distance < 0) {output << "null";}
    else {output << '"' << std::setprecision(12) << result.distance << '"';}
    output << "}\n";
}

static void WriteComponentsJson(std::ostream& output, const GraphComponents& groups) {
    output << "{\"groups\":[";
    for (int index = 0; index < groups.GetLength(); index++) {
        if (index > 0) {output << ',';}
        WriteVertices(output, groups.Get(index));
    }
    output << "]}\n";
}

static void WriteTimingsJson(std::ostream& output, const RouteTimings& times) {
    output << "{\"dijkstra_us\":" << times.dijkstra
           << ",\"bellman_ford_us\":" << times.bellmanFord << "}\n";
}

static void CheckEnd(std::istream& input, const char* error) {
    std::string extra;
    if (input >> extra) {throw std::invalid_argument(error);}
}

static std::string ReadParameter(std::istream& input) {
    std::string value;
    if (!(input >> value)) {throw std::invalid_argument("Пропущен параметр команды");}
    return value;
}

static CampusGraph ReadAdapterGraph() {
    CampusGraph graph = ReadGraph(std::cin);
    CheckEnd(std::cin, "После последней дорожки есть лишние данные");
    return graph;
}

int RunGraphAdapter() {
    const char* extraParameters = "Лишние параметры команды";
    try {
        std::string line;
        if (!std::getline(std::cin, line)) {return 0;}
        std::istringstream parameters(line);
        std::string command = ReadParameter(parameters);

        if (command == "demo") {
            CheckEnd(parameters, extraParameters);
            WriteGraph(std::cout, DemoGraph());
        } else if (command == "generate") {
            auto topology = ParseTopology(ReadParameter(parameters));
            int size = ParseInt(ReadParameter(parameters));
            unsigned int seed = ParseSeed(ReadParameter(parameters));
            int groups = topology == GraphTopology::SeparateChains
                ? ParseInt(ReadParameter(parameters))
                : 2;
            CheckEnd(parameters, extraParameters);
            CheckCampusSize(size);
            WriteGraph(std::cout, GenerateGraph(topology, size, seed, groups));
        } else if (command == "show") {
            CheckEnd(parameters, extraParameters);
            WriteGraphJson(std::cout, ReadAdapterGraph());
        } else if (command == "route") {
            auto algorithm = ParseAlgorithm(ReadParameter(parameters));
            int source = ParseInt(ReadParameter(parameters));
            int destination = ParseInt(ReadParameter(parameters));
            CheckEnd(parameters, extraParameters);
            WriteRouteJson(std::cout, FindRoute(ReadAdapterGraph(), algorithm, source, destination));
        } else if (command == "components") {
            CheckEnd(parameters, extraParameters);
            WriteComponentsJson(std::cout, GraphSearch::FindComponents(ReadAdapterGraph()));
        } else if (command == "compare") {
            int source = ParseInt(ReadParameter(parameters));
            int repeats = ParseInt(ReadParameter(parameters));
            CheckEnd(parameters, extraParameters);
            WriteTimingsJson(std::cout, CompareRoutes(ReadAdapterGraph(), source, repeats));
        } else if (command == "performance") {
            int maxSize = ParseInt(ReadParameter(parameters));
            int repeats = ParseInt(ReadParameter(parameters));
            unsigned int seed = ParseSeed(ReadParameter(parameters));
            CheckEnd(parameters, extraParameters);
            WritePerformance(std::cout, maxSize, repeats, seed);
        } else {
            throw std::invalid_argument("Неизвестная операция со схемой");
        }
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
