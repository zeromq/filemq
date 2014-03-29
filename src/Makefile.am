lib_LTLIBRARIES = libfmq.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libfmq.pc

include_HEADERS = \
    ../include/fmq.h \
    ../include/fmq_client.h \
    ../include/fmq_msg.h \
    ../include/fmq_server.h

libfmq_la_SOURCES = \
    fmq_client.c \
    fmq_msg.c \
    fmq_server.c 

INCLUDES = -I$(top_srcdir)/include
bin_PROGRAMS = filemq track fmq_selftest
fmq_selftest_LDADD = libfmq.la
filemq_LDADD = libfmq.la
track_LDADD = libfmq.la

if ON_MINGW
libfmq_la_LDFLAGS = -no-undefined -avoid-version -version-info @LTVER@
else
libfmq_la_LDFLAGS = -version-info @LTVER@
endif

TESTS = fmq_selftest

# Produce generated models; do this manually in src directory
code:
	gsl -q fmq_msg.xml
	gsl -q -script:server_c -trace:0 -animate:0 fmq_server.xml
	gsl -q -script:client_c -trace:0 -animate:0 fmq_client.xml
