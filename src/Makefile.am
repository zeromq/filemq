lib_LTLIBRARIES = libfmq.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libfmq.pc

include_HEADERS = \
    ../include/fmq.h

libfmq_la_SOURCES = \
    fmq_chunk.c \
    fmq_client.c \
    fmq_config.c \
    fmq_dir.c \
    fmq_file.c \
    fmq_msg.c \
    fmq_patch.c \
    fmq_sasl.c \
    fmq_server.c \
    fmq_sha.c \
    ../sha-1.1/sha1.c \
    ../sha-1.1/sha256.c \
    ../sha-1.1/sha384.c \
    ../sha-1.1/sha512.c \
    ../sha-1.1/hmac-sha1.c \
    ../sha-1.1/hmac-sha256.c \
    ../sha-1.1/hmac-sha384.c \
    ../sha-1.1/hmac-sha512.c

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

