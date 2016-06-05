# Compiler & linker flags
AM_CXXFLAGS = -g -Wall -Werror -DENABLE_DEBUG
AM_LDFLAGS  =

# Unit test scripts
TESTS = \
    trie.sh \
    struct_test.sh \
    benchmark.sh


# Unit test programs
check_PROGRAMS = \
    benchmark \
    paths \
    trie

benchmark_SOURCES = \
    benchmark.cxx

paths_SOURCES = \
    paths.cxx

trie_SOURCES = \
    trie.cxx
