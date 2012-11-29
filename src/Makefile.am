lib_LTLIBRARIES = libfmq.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libfmq.pc

include_HEADERS = \
    ../include/fmq.h \
    ../include/fmq_chunk.h \
    ../include/fmq_client.h \
    ../include/fmq_config.h \
    ../include/fmq_dir.h \
    ../include/fmq_file.h \
    ../include/fmq_hash.h \
    ../include/fmq_msg.h \
    ../include/fmq_patch.h \
    ../include/fmq_sasl.h \
    ../include/fmq_server.h

libfmq_la_SOURCES = \
    fmq_chunk.c \
    fmq_client.c \
    fmq_config.c \
    fmq_dir.c \
    fmq_file.c \
    fmq_hash.c \
    fmq_msg.c \
    fmq_patch.c \
    fmq_sasl.c \
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

