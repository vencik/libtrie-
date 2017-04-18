# Compiler & linker flags
AM_CXXFLAGS = -g -Wall -Werror
AM_LDFLAGS  =

# Enable debug messages
if ENABLE_DEBUG
AM_CXXFLAGS += -DENABLE_DEBUG
endif


SUBDIRS = \
    libtriexx \
    unit_test


# Internal headers
noinst_HEADERS = \
    config.hxx


# No library so far
#lib_LTLIBRARIES = \
#    libtrie++.la
#
#libtrie++_la_SOURCES =
#
#libtrie++_la_LIBADD =
#
#LDADD = libtrie++.la


# Executables (none so far)
#bin_PROGRAMS = \
#    libtrie++_something
#
#libtrie++_something_SOURCES = \
#    something.cxx
