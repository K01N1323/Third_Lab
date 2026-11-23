CXX = c++
PYTHON ?= python3
CPPFLAGS = -I.
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic
SOURCES = Src/main.cpp Ui/Menu.cpp Ui/GraphIO.cpp
ADAPTER_SOURCES = Src/gui_adapter.cpp Ui/GraphAdapter.cpp Ui/GraphIO.cpp
TEST_SOURCES = Tests/main.cpp Ui/GraphIO.cpp
HEADERS = $(wildcard Include/*.hpp BaseStructures/*.hpp Sequences/*.hpp Tests/*.hpp Ui/*.hpp)
TARGET = build/release/campus_routes
ADAPTER_TARGET = build/release/graph_adapter
TEST_TARGET = build/tests/campus_routes_tests
SANITIZER_TARGET = build/sanitize/campus_routes_tests
MAX ?= 256
REPEATS ?= 5
SEED ?= 42
CSV ?= build/performance.csv

.PHONY: all run test sanitize release performance clean

all: release

$(TARGET): $(SOURCES) $(HEADERS) Makefile
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -O2 $(SOURCES) -o $@

$(ADAPTER_TARGET): $(ADAPTER_SOURCES) $(HEADERS) Makefile
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -O2 $(ADAPTER_SOURCES) -o $@

release: $(TARGET) $(ADAPTER_TARGET)

run:
	$(PYTHON) gui.py

$(TEST_TARGET): $(TEST_SOURCES) $(HEADERS) Makefile
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -O2 $(TEST_SOURCES) -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(SANITIZER_TARGET): $(TEST_SOURCES) $(HEADERS) Makefile
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined $(TEST_SOURCES) -o $@

sanitize: $(SANITIZER_TARGET)
	./$(SANITIZER_TARGET)

performance: $(ADAPTER_TARGET)
	@directory=$$(dirname "$(CSV)"); \
	mkdir -p "$$directory" && \
	temporary=$$(mktemp "$$directory/.campus-performance.XXXXXX") && \
	trap 'rm -f "$$temporary"' EXIT && \
	printf 'performance %s %s %s\n' "$(MAX)" "$(REPEATS)" "$(SEED)" | ./$(ADAPTER_TARGET) > "$$temporary" && \
	mv -f "$$temporary" "$(CSV)"

clean:
	rm -rf build
