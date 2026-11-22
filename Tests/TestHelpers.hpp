#ifndef TEST_HELPERS_HPP
#define TEST_HELPERS_HPP

#include <iostream>

template <typename TException, typename TFunction>
bool TestThrows(TFunction function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

inline void CheckGraph(bool condition, const char* name, int& tests_passed, int& tests_failed) {
    if (condition) {
        tests_passed++;
        std::cout << "  Успешно: " << name << '\n';
    } else {
        tests_failed++;
        std::cout << "  Провалено: " << name << '\n';
    }
}

#endif // TEST_HELPERS_HPP
