#ifndef GRAPH_IO_HPP
#define GRAPH_IO_HPP

#include <istream>
#include <ostream>
#include <string>

#include "Include/RouteMeasurement.hpp"

const int MaxCampusSize = 2048;

enum class RouteAlgorithm {
    Dijkstra,
    BellmanFord
};

struct RouteResult {
    MutableArraySequence<int> path;
    long double distance;
};

CampusGraph ReadGraph(std::istream& input);
void WriteGraph(std::ostream& output, const CampusGraph& graph);
CampusGraph DemoGraph();
void CheckCampusSize(int count);
int ParseInt(const std::string& text);
unsigned int ParseSeed(const std::string& text);
GraphTopology ParseTopology(const std::string& name);
RouteAlgorithm ParseAlgorithm(const std::string& name);

RouteResult FindRoute(const CampusGraph& graph, RouteAlgorithm algorithm, int source, int destination);
void WritePerformance(std::ostream& output, int maxSize, int repeats, unsigned int seed);

#endif // GRAPH_IO_HPP
