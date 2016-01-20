Format:         1.0
Source:         filemq
Version:        2.0.0-1
Binary:         libfilemq2, filemq-dev
Architecture:   any all
Maintainer:     John Doe <John.Doe@example.com>
Standards-Version: 3.9.5
Build-Depends: bison, debhelper (>= 8),
    pkg-config,
    automake,
    autoconf,
    libtool,
    libsodium-dev,
    libzmq4-dev,
    libczmq-dev,
    dh-autoreconf

Package-List:
 libfilemq2 deb net optional arch=any
 filemq-dev deb libdevel optional arch=any

