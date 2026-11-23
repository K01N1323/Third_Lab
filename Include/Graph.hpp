#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <cmath>
#include <stdexcept>

#include "BaseStructures/SquareMatrix.hpp"
#include "Sequences/MutableArraySequence.hpp"

template <typename TVertex, typename TWeight>
class Graph {
public:
    struct Edge {
        int from;
        int to;
        TWeight weight;
    };

private:
    struct EdgeCell {
        bool exists = false;
        TWeight weight = TWeight();
    };

    SquareMatrix<EdgeCell> edges; // матрица смежности 
    DynamicArray<TVertex> vertices; // вершины

public:
    explicit Graph(int count): edges(count), vertices(count) {}

    int GetVertexCount() const {
        return vertices.GetSize();
    }

    void CheckVertex(int index) const {
        if (index < 0 || index >= this->GetVertexCount()) {
            throw std::out_of_range("Вершина с таким номером отсутствует или неправильный индекс");
        }
    }

    const TVertex& GetVertex(int index) const {
        return vertices.Get(index);
    }

    void SetVertex(int index, const TVertex& value) {
        vertices.Set(index, value);
    }

    const TWeight* FindWeight(int from, int to) const {
        const EdgeCell& cell = edges.GetIJ(from, to);
        return cell.exists ? &cell.weight : nullptr;
    }

    void AddEdge(int from, int to, TWeight weight) {
        if (from == to) {
            this->CheckVertex(from);
            throw std::invalid_argument("Дорожка должна соединять разные вершины");
        }
        if (!std::isfinite(weight) || weight < 0) {
            throw std::invalid_argument("Длина дорожки должна быть конечной и неотрицательной");
        }
        if (this->FindWeight(from, to) != nullptr) {
            throw std::invalid_argument("Дорожка между этими вершинами уже задана");
        }

        EdgeCell cell{true, weight};
        edges.SetIJ(from, to, cell);
        edges.SetIJ(to, from, cell);
    }

    void RemoveEdge(int from, int to) {
        edges.SetIJ(from, to, EdgeCell());
        edges.SetIJ(to, from, EdgeCell());
    }

    // Читаю верхний треугольник, чтобы каждое ребро встретилось один раз.
    MutableArraySequence<Edge> GetEdges() const {
        MutableArraySequence<Edge> result;
        for (int from = 0; from < this->GetVertexCount(); from++) {
            for (int to = from + 1; to < this->GetVertexCount(); to++) {
                const TWeight* weight = this->FindWeight(from, to);
                if (weight != nullptr) {result.Append({from, to, *weight});}
            }
        }
        return result;
    }
};

#endif // GRAPH_HPP
