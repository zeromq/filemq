lib_LTLIBRARIES = libfmq.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libfmq.pc

include_HEADERS = \
    ../include/fmq.h

libfmq_la_SOURCES = 

INCLUDES = -I$(top_srcdir)/include
bin_PROGRAMS = filemq
filemq_LDADD = libfmq.la

if ON_MINGW
libfmq_la_LDFLAGS = -no-undefined -avoid-version -version-info @LTVER@
else
libfmq_la_LDFLAGS = -version-info @LTVER@
endif

TESTS = 


