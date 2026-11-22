#ifndef SQUARE_MATRIX_HPP
#define SQUARE_MATRIX_HPP

#include <limits>
#include <stdexcept>

#include "BaseStructures/DynamicArray.hpp"

// Хранение и доступ по строке и столбцу из матричной лабораторной.
template <class T> class SquareMatrix {
private:
    int size;
    DynamicArray<T> data;

    static int GetDataSize(int size) {
        if (size < 0 || (size > 0 && size > std::numeric_limits<int>::max() / size)) {
            throw std::invalid_argument("Некорректный размер квадратной матрицы");
        }
        return size * size;
    }

    int GetIndex(int row, int col) const {
        if (row < 0 || row >= size || col < 0 || col >= size) {
            throw std::out_of_range("Индекс вне границ матрицы");
        }
        return row * size + col;
    }

public:
    explicit SquareMatrix(int size): size(size), data(GetDataSize(size)) {}

    const T& GetIJ(int row, int col) const {
        return data.Get(this->GetIndex(row, col));
    }

    void SetIJ(int row, int col, const T& item) {
        data.Set(this->GetIndex(row, col), item);
    }
};

#endif // SQUARE_MATRIX_HPP
