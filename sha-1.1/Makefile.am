AUTOMAKE_OPTIONS = foreign
AM_CFLAGS = -Wall

lib_LIBRARIES = libsha.a
libsha_a_SOURCES = sha.c sha1.c sha256.c sha384.c sha512.c \
	hmac-sha1.c hmac-sha256.c hmac-sha384.c hmac-sha512.c

bin_PROGRAMS = sha
noinst_PROGRAMS = shatest

sha_SOURCES = sha.c sha1.c sha256.c sha384.c sha512.c \
	hmac-sha1.c hmac-sha256.c hmac-sha384.c hmac-sha512.c \
	sha1.h sha256.h sha384.h sha512.h \
	hmac-sha1.h hmac-sha256.h hmac-sha384.h hmac-sha512.h \
	version.h
shatest_SOURCES = shatest.c sha1.c sha256.c sha384.c sha512.c \
	sha1.h sha256.h sha384.h sha512.h

man_MANS = sha.1

EXTRA_DIST = $(man_MANS) README.SHA
