#ifndef GRAPH_ALGORITHMS_HPP
#define GRAPH_ALGORITHMS_HPP

#include <cmath>
#include <stdexcept>

#include "Graph.hpp"

class ShortestPaths {
private:
    DynamicArray<long double> distances;
    DynamicArray<int> previous;

public:
    ShortestPaths(int count, int source): distances(count), previous(count) {
        for (int vertex = 0; vertex < count; vertex++) {
            distances.Set(vertex, -1);
            previous.Set(vertex, -1);
        }
        distances.Set(source, 0);
    }

    int GetCount() const {
        return distances.GetSize();
    }

    // Для недостижимой вершины возвращаю -1. Длина пути может быть равна нулю.
    long double GetDistance(int vertex) const {
        return distances.Get(vertex);
    }

    MutableArraySequence<int> GetPath(int destination) const {
        MutableArraySequence<int> path;
        if (this->GetDistance(destination) < 0) {return path;}

        MutableArraySequence<int> reversed;
        for (int vertex = destination; vertex != -1; vertex = previous.Get(vertex)) {
            reversed.Append(vertex);
        }
        for (int index = reversed.GetLength() - 1; index >= 0; index--) {
            path.Append(reversed.Get(index));
        }
        return path;
    }

    // Меняю предшественника только при строгом улучшении.
    bool RelaxEdge(int from, int to, long double weight, bool& overflow) {
        long double current = this->GetDistance(from);
        long double best = this->GetDistance(to);
        if (current < 0 || (best >= 0 && current >= best)) {return false;}

        long double candidate = current + weight;
        if (!std::isfinite(candidate)) {
            overflow = true;
            return false;
        }
        if (best >= 0 && candidate >= best) {return false;}

        distances.Set(to, candidate);
        previous.Set(to, from);
        return true;
    }
};

using GraphComponent = MutableArraySequence<int>;
using GraphComponents = MutableArraySequence<GraphComponent>;

namespace GraphSearch {

// Список группы одновременно служит очередью обхода.
template <typename TVertex, typename TWeight>
GraphComponent CollectComponent(const Graph<TVertex, TWeight>& graph, int start,
                                DynamicArray<bool>& visited) {
    GraphComponent group;
    group.Append(start);
    visited.Set(start, true);

    for (int index = 0; index < group.GetLength(); index++) {
        int vertex = group.Get(index);
        for (int next = 0; next < graph.GetVertexCount(); next++) {
            if (!visited.Get(next) && graph.FindWeight(vertex, next) != nullptr) {
                visited.Set(next, true);
                group.Append(next);
            }
        }
    }
    return group;
}

template <typename TVertex, typename TWeight>
GraphComponents FindComponents(const Graph<TVertex, TWeight>& graph) {
    DynamicArray<bool> visited(graph.GetVertexCount());
    GraphComponents groups;
    for (int vertex = 0; vertex < graph.GetVertexCount(); vertex++) {
        if (!visited.Get(vertex)) {
            groups.Append(CollectComponent(graph, vertex, visited));
        }
    }
    return groups;
}

template <typename TVertex, typename TWeight>
void CheckDistances(const Graph<TVertex, TWeight>& graph, int source,
                    const ShortestPaths& paths, bool overflow) {
    if (!overflow) {return;}

    DynamicArray<bool> visited(graph.GetVertexCount());
    GraphComponent group = CollectComponent(graph, source, visited);
    for (int index = 0; index < group.GetLength(); index++) {
        if (paths.GetDistance(group.Get(index)) < 0) {
            throw std::runtime_error("Длина кратчайшего пути выходит за диапазон");
        }
    }
}

template <typename TVertex, typename TWeight>
ShortestPaths Dijkstra(const Graph<TVertex, TWeight>& graph, int source) {
    graph.CheckVertex(source);
    int count = graph.GetVertexCount();
    ShortestPaths paths(count, source);
    DynamicArray<bool> visited(count);
    bool overflow = false;

    for (int step = 0; step < count; step++) {
        int closest = -1;
        for (int vertex = 0; vertex < count; vertex++) {
            long double distance = paths.GetDistance(vertex);
            if (!visited.Get(vertex) && distance >= 0 &&
                (closest == -1 || distance < paths.GetDistance(closest))) {
                closest = vertex;
            }
        }
        if (closest == -1) {break;}

        visited.Set(closest, true);
        for (int next = 0; next < count; next++) {
            const TWeight* weight = graph.FindWeight(closest, next);
            if (!visited.Get(next) && weight != nullptr) {
                paths.RelaxEdge(closest, next, *weight, overflow);
            }
        }
    }
    CheckDistances(graph, source, paths, overflow);
    return paths;
}

template <typename TVertex, typename TWeight>
ShortestPaths BellmanFord(const Graph<TVertex, TWeight>& graph, int source) {
    graph.CheckVertex(source);
    int count = graph.GetVertexCount();
    ShortestPaths paths(count, source);
    auto edges = graph.GetEdges();
    bool overflow = false;

    for (int pass = 1; pass < count; pass++) {
        bool changed = false;
        for (int index = 0; index < edges.GetLength(); index++) {
            const auto& edge = edges.Get(index);
            if (paths.RelaxEdge(edge.from, edge.to, edge.weight, overflow)) {changed = true;}
            if (paths.RelaxEdge(edge.to, edge.from, edge.weight, overflow)) {changed = true;}
        }
        if (!changed) {break;}
    }
    CheckDistances(graph, source, paths, overflow);
    return paths;
}

} // namespace GraphSearch

#endif // GRAPH_ALGORITHMS_HPP
