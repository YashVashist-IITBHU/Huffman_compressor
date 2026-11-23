CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES := -Iinclude

LIB_SRC := src/BitIO.cpp src/HuffmanTree.cpp src/HuffmanCoder.cpp
HEADERS := $(wildcard include/*.hpp)

.PHONY: all test bench clean

all: bin/huffman bin/tests bin/gen_samples

bin:
	mkdir -p bin

bin/huffman: $(LIB_SRC) src/main.cpp $(HEADERS) | bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIB_SRC) src/main.cpp -o $@

bin/tests: $(LIB_SRC) tests/test_huffman.cpp $(HEADERS) | bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIB_SRC) tests/test_huffman.cpp -o $@

bin/gen_samples: tools/gen_samples.cpp | bin
	$(CXX) $(CXXFLAGS) tools/gen_samples.cpp -o $@

test: bin/tests
	./bin/tests

bench: all
	./scripts/benchmark.sh

clean:
	rm -rf bin build samples
