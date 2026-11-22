#ifndef ROUTE_MEASUREMENT_HPP
#define ROUTE_MEASUREMENT_HPP

#include <chrono>
#include <utility>
#include <stdexcept>

#include "GraphAlgorithms.hpp"
#include "GraphGenerator.hpp"

struct RouteTimings {
    double dijkstra;
    double bellmanFord;
};

inline bool EqualDistances(const ShortestPaths& left, const ShortestPaths& right) {
    if (left.GetCount() != right.GetCount()) {return false;}

    for (int vertex = 0; vertex < left.GetCount(); vertex++) {
        long double first = left.GetDistance(vertex);
        long double second = right.GetDistance(vertex);
        if (first == second) {continue;}
        if (first < 0 || second < 0) {return false;}

        if (first < second) {std::swap(first, second);}
        if (first - second > first * 1e-12L) {return false;}
    }
    return true;
}

inline double Median(DynamicArray<double>& times) {
    for (int index = 1; index < times.GetSize(); index++) {
        double value = times.Get(index);
        int position = index;
        while (position > 0 && value < times.Get(position - 1)) {
            times.Set(position, times.Get(position - 1));
            position--;
        }
        times.Set(position, value);
    }

    int middle = times.GetSize() / 2;
    if (times.GetSize() % 2 != 0) {return times.Get(middle);}
    return (times.Get(middle - 1) + times.Get(middle)) / 2;
}

inline void CheckRepeats(int repeats) {
    if (repeats < 1 || repeats > 50) {
        throw std::invalid_argument("Число повторов должно быть от 1 до 50");
    }
}

inline ShortestPaths MeasureSearch(ShortestPaths (*search)(const CampusGraph&, int),
                                  const CampusGraph& graph, int source, double& time) {
    auto begin = std::chrono::steady_clock::now();
    ShortestPaths paths = search(graph, source);
    auto end = std::chrono::steady_clock::now();
    time = std::chrono::duration<double, std::milli>(end - begin).count() * 1000;
    return paths;
}

inline RouteTimings CompareRoutes(const CampusGraph& graph, int source, int repeats) {
    CheckRepeats(repeats);
    DynamicArray<double> dijkstraTimes(repeats);
    DynamicArray<double> bellmanFordTimes(repeats);

    auto dijkstra = GraphSearch::Dijkstra<std::string, double>;
    auto bellmanFord = GraphSearch::BellmanFord<std::string, double>;
    for (int repeat = 0; repeat < repeats; repeat++) {
        bool dijkstraFirst = repeat % 2 == 0;
        double firstTime = 0;
        double secondTime = 0;
        ShortestPaths first = MeasureSearch(dijkstraFirst ? dijkstra : bellmanFord, graph, source, firstTime);
        ShortestPaths second = MeasureSearch(dijkstraFirst ? bellmanFord : dijkstra, graph, source, secondTime);
        if (!EqualDistances(first, second)) {throw std::runtime_error("Результаты поиска разошлись");}
        dijkstraTimes.Set(repeat, dijkstraFirst ? firstTime : secondTime);
        bellmanFordTimes.Set(repeat, dijkstraFirst ? secondTime : firstTime);
    }

    return {Median(dijkstraTimes), Median(bellmanFordTimes)};
}

#endif // ROUTE_MEASUREMENT_HPP
