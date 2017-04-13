# Compiler & linker flags
AM_CXXFLAGS = -g -Wall -Werror -DENABLE_DEBUG
AM_LDFLAGS  =

# Unit test scripts that use Python v3
PYTHON3_TESTS = \
    struct_test.sh


# Unit test scripts
TESTS = \
    trie.sh \
    benchmark.sh

if ENABLE_PYTHON3_UTS
TESTS += $(PYTHON3_TESTS)
endif


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
