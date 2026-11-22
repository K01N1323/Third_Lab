#include "Menu.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "GraphIO.hpp"

static std::string ReadArgument(const char* prompt) {
    std::cout << prompt << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) {return {};}
    std::istringstream input(line);
    std::string value;
    std::string extra;
    if (!(input >> value) || (input >> extra)) {
        throw std::invalid_argument("Введите одно значение");
    }
    return value;
}

static void PrintRoute(const CampusGraph& graph, const RouteResult& result) {
    if (result.distance < 0) {
        std::cout << "Маршрут недоступен.\n";
        return;
    }
    for (int index = 0; index < result.path.GetLength(); index++) {
        if (index != 0) {std::cout << " → ";}
        std::cout << graph.GetVertex(result.path.Get(index));
    }
    std::cout << "\nДлина маршрута — " << std::setprecision(12) << result.distance << " м.\n";
}

int RunMenu() {
    CampusGraph graph = DemoGraph();
    while (std::cin) {
        std::cout << "\nМаршруты по кампусу\n"
                  << "1. Показать схему\n2. Ввести схему\n3. Сгенерировать схему\n"
                  << "4. Найти маршрут\n5. Показать связанные группы\n"
                  << "6. Сравнить время поиска\n7. Вернуть пример\n0. Выход\n";
        try {
            std::string option = ReadArgument("Выберите действие: ");
            if (option.empty() || option == "0") {return 0;}
            if (option == "1") {
                std::cout << "Количество корпусов и дорожек, названия корпусов, затем дорожки.\n";
                WriteGraph(std::cout, graph);
            } else if (option == "2") {
                std::cout << "Введите число корпусов и дорожек, затем названия по одному на строку.\n"
                          << "Далее для каждой дорожки введите два номера от 0 и длину в метрах.\n";
                graph = ReadGraph(std::cin);
            } else if (option == "3") {
                auto topology = ParseTopology(ReadArgument("Топология (chain, cycle, separate): "));
                int size = ParseInt(ReadArgument("Количество корпусов: "));
                CheckCampusSize(size);
                unsigned int seed = ParseSeed(ReadArgument("Начальное число генератора: "));
                int groups = topology == GraphTopology::SeparateChains
                    ? ParseInt(ReadArgument("Количество групп: "))
                    : 2;
                graph = GenerateGraph(topology, size, seed, groups);
            } else if (option == "4") {
                auto algorithm = ParseAlgorithm(ReadArgument("Алгоритм (dijkstra, bellman-ford): "));
                int source = ParseInt(ReadArgument("Номер начального корпуса: "));
                int destination = ParseInt(ReadArgument("Номер конечного корпуса: "));
                PrintRoute(graph, FindRoute(graph, algorithm, source, destination));
            } else if (option == "5") {
                auto groups = GraphSearch::FindComponents(graph);
                std::cout << "Количество групп — " << groups.GetLength() << '\n';
                for (int index = 0; index < groups.GetLength(); index++) {
                    std::cout << index + 1 << '.';
                    const auto& group = groups.Get(index);
                    for (int position = 0; position < group.GetLength(); position++) {
                        std::cout << ' ' << graph.GetVertex(group.Get(position));
                    }
                    std::cout << '\n';
                }
            } else if (option == "6") {
                int source = ParseInt(ReadArgument("Номер начального корпуса: "));
                int repeats = ParseInt(ReadArgument("Число повторов: "));
                auto times = CompareRoutes(graph, source, repeats);
                std::cout << "Медианное время, мкс\n"
                          << "Дейкстра — " << times.dijkstra << '\n'
                          << "Беллман–Форд — " << times.bellmanFord << '\n';
            } else if (option == "7") {
                graph = DemoGraph();
            } else {
                std::cout << "Неизвестный пункт меню.\n";
            }
        } catch (const std::exception& error) {
            if (!std::cin.eof()) {std::cerr << "Ошибка: " << error.what() << '\n';}
        }
    }
    return 0;
}
