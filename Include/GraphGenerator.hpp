#ifndef GRAPH_GENERATOR_HPP
#define GRAPH_GENERATOR_HPP

#include <random>
#include <stdexcept>
#include <string>

#include "Graph.hpp"

using CampusGraph = Graph<std::string, double>;

enum class GraphTopology {
    Chain,
    Cycle,
    SeparateChains
};

inline CampusGraph GenerateGraph(GraphTopology topology, int count, unsigned int seed, int groups = 2) {
    if (count < 0) {throw std::invalid_argument("Количество корпусов должно быть неотрицательным");}
    if (topology == GraphTopology::Cycle && count < 3) {
        throw std::invalid_argument("Для кольца нужны хотя бы три корпуса");
    }
    if (topology == GraphTopology::SeparateChains && (groups < 2 || groups > count)) {
        throw std::invalid_argument("Количество групп должно быть от 2 до количества корпусов");
    }
    if (topology != GraphTopology::Chain && topology != GraphTopology::Cycle &&
        topology != GraphTopology::SeparateChains) {
        throw std::invalid_argument("Неизвестная топология");
    }

    CampusGraph graph(count);
    for (int vertex = 0; vertex < count; vertex++) {
        graph.SetVertex(vertex, "Корпус " + std::to_string(vertex));
    }

    std::mt19937 engine(seed);
    std::uniform_int_distribution<int> distance(10, 150);
    int groupCount = topology == GraphTopology::SeparateChains ? groups : 1;
    int first = 0;

    for (int group = 0; group < groupCount; group++) {
        int size = count / groupCount + (group < count % groupCount ? 1 : 0);
        for (int vertex = first + 1; vertex < first + size; vertex++) {
            graph.AddEdge(vertex - 1, vertex, distance(engine));
        }
        first += size;
    }

    if (topology == GraphTopology::Cycle) {graph.AddEdge(count - 1, 0, distance(engine));}
    return graph;
}

#endif // GRAPH_GENERATOR_HPP
