CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -Iinclude
BINDIR   := bin

.PHONY: all clean test stress bench

all: $(BINDIR)/strset $(BINDIR)/strset_naive $(BINDIR)/test_unit $(BINDIR)/test_fuzz

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/strset: src/main.cpp include/string_set.hpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp

$(BINDIR)/strset_naive: src/main_naive.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ src/main_naive.cpp

$(BINDIR)/test_unit: tests/test_unit.cpp include/string_set.hpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ tests/test_unit.cpp

$(BINDIR)/test_fuzz: tests/test_fuzz.cpp include/string_set.hpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ tests/test_fuzz.cpp

test: $(BINDIR)/test_unit $(BINDIR)/test_fuzz
	$(BINDIR)/test_unit
	$(BINDIR)/test_fuzz

stress: all
	tests/stress_test.sh

bench: all
	tools/run_benchmarks.sh

clean:
	rm -rf $(BINDIR)
